use axum::{
    extract::{
        ws::{Message, WebSocket, WebSocketUpgrade},
        Request, State,
    },
    http::StatusCode,
    response::{IntoResponse, Response},
};
use std::sync::{Arc, RwLock};
use std::time::{Duration, Instant};
use tokio::sync::mpsc;
use tracing::{info, warn};

const FRAME_ACK_TIMEOUT: Duration = Duration::from_secs(15);
const MAX_FRAME_RETRIES: u8 = 3;

use crate::{
    media::decode_image,
    models::{AppContext, SharedContext},
    processing::{fit, transform},
    routes::image_bytes,
};

pub async fn ws_handler(
    ws: WebSocketUpgrade,
    State(SharedContext(state)): State<SharedContext>,
) -> Response {
    ws.on_upgrade(|socket| handle_socket(socket, state))
}

async fn handle_socket(mut socket: WebSocket, state: Arc<RwLock<AppContext>>) {
    info!("ESP32/Émulateur connecté au WebSocket !");

    // Création du canal
    let (tx, mut rx) = mpsc::channel::<Vec<Vec<u8>>>(8);

    // Stockage de `tx` dans le state partagé
    let connection_id = {
        let mut ctx = state.write().unwrap();
        ctx.ws_connection_id = ctx.ws_connection_id.wrapping_add(1);
        let connection_id = ctx.ws_connection_id;
        ctx.ws_tx = Some(tx);
        connection_id
    };

    let mut current_frames: Vec<Vec<u8>> = Vec::new();
    let mut queued_frames: Option<Vec<Vec<u8>>> = None;
    let mut next_frame_idx: usize = 0;
    let mut frame_in_flight = false;
    let mut in_flight_idx: Option<usize> = None;
    let mut frame_retries = 0;
    let mut heartbeat = tokio::time::interval_at(
        tokio::time::Instant::now() + Duration::from_secs(10),
        Duration::from_secs(10),
    );
    let mut frame_timeout = tokio::time::interval(Duration::from_secs(5));
    let mut last_pong = Instant::now();
    let mut frame_sent_at: Option<Instant> = None;

    loop {
        tokio::select! {
            _ = heartbeat.tick() => {
                if last_pong.elapsed() > Duration::from_secs(20) {
                    warn!("WebSocket inactif depuis plus de 20 secondes, fermeture");
                    break;
                }
                if socket.send(Message::Ping(Vec::new().into())).await.is_err() {
                    info!("Impossible d'envoyer le heartbeat WebSocket");
                    break;
                }
            }

            _ = frame_timeout.tick() => {
                if frame_in_flight
                    && frame_sent_at.is_some_and(|sent_at| sent_at.elapsed() > FRAME_ACK_TIMEOUT)
                {
                    if let Some(frame_idx) = in_flight_idx {
                        if frame_retries < MAX_FRAME_RETRIES {
                            frame_retries += 1;
                            warn!(
                                "ACK absent pour la frame {} (tentative {}/{}), retransmission",
                                frame_idx + 1,
                                frame_retries,
                                MAX_FRAME_RETRIES
                            );
                            if send_frame_at(
                                &mut socket,
                                &current_frames,
                                frame_idx,
                                connection_id,
                            )
                            .await
                            .is_err()
                            {
                                break;
                            }
                            frame_sent_at = Some(Instant::now());
                        } else {
                            warn!("Aucun ACK reçu après {} retransmissions, fermeture du WebSocket", MAX_FRAME_RETRIES);
                            break;
                        }
                    }
                }
            }

            // Réception d'une nouvelle animation envoyée par `process_for_esp32`
            Some(new_frames) = rx.recv() => {
                info!("Nouvelles frames reçues dans la boucle WS ({})", new_frames.len());
                if new_frames.is_empty() {
                    warn!("Animation vide ignorée");
                    continue;
                }

                // Une seule frame doit être en transit à la fois : l'ESP32
                // confirme chaque frame avant de recevoir la suivante.
                if !frame_in_flight {
                    current_frames = new_frames;
                    next_frame_idx = 0;
                    if send_frame_at(
                        &mut socket,
                        &current_frames,
                        next_frame_idx,
                        connection_id,
                    )
                    .await
                    .is_err()
                    {
                        break;
                    }
                    next_frame_idx += 1;
                    frame_in_flight = true;
                    in_flight_idx = Some(next_frame_idx - 1);
                    frame_retries = 0;
                    frame_sent_at = Some(Instant::now());
                } else {
                    // Keep only the newest animation. It will replace the old
                    // one after the frame currently displayed is acknowledged.
                    queued_frames = Some(new_frames);
                }
            }

            // Réception du "ACK" depuis le Python/ESP32
            message = socket.recv() => {
                match message {
                    Some(Ok(Message::Text(text))) if text == "ACK" => {
                        if !frame_in_flight {
                            warn!("ACK inattendu ignoré");
                            continue;
                        }
                        info!(
                            "WS ACK reçu (connexion {}, prochaine frame: {}/{})",
                            connection_id,
                            (next_frame_idx % current_frames.len().max(1)) + 1,
                            current_frames.len()
                        );
                        frame_in_flight = false;
                        in_flight_idx = None;
                        frame_sent_at = None;
                        frame_retries = 0;
                        let replaced_animation = if let Some(new_frames) = queued_frames.take() {
                            current_frames = new_frames;
                            next_frame_idx = 0;
                            true
                        } else {
                            false
                        };
                        if replaced_animation || current_frames.len() > 1 {
                            if next_frame_idx >= current_frames.len() {
                                next_frame_idx = 0;
                            }
                            if send_frame_at(
                                &mut socket,
                                &current_frames,
                                next_frame_idx,
                                connection_id,
                            )
                            .await
                            .is_err()
                            {
                                break;
                            }
                            next_frame_idx += 1;
                            frame_in_flight = true;
                            in_flight_idx = Some(next_frame_idx - 1);
                            frame_sent_at = Some(Instant::now());
                        }
                    }
                    Some(Ok(Message::Ping(payload))) => {
                        if socket.send(Message::Pong(payload)).await.is_err() {
                            break;
                        }
                    }
                    Some(Ok(Message::Pong(_))) => {
                        last_pong = Instant::now();
                    }
                    Some(Ok(Message::Text(text))) if text == "NACK" => {
                        if let Some(frame_idx) = in_flight_idx {
                            if frame_retries < MAX_FRAME_RETRIES {
                                frame_retries += 1;
                                warn!(
                                    "Frame {} rejetée (tentative {}/{}), retransmission",
                                    frame_idx + 1,
                                    frame_retries,
                                    MAX_FRAME_RETRIES
                                );
                                if send_frame_at(
                                    &mut socket,
                                    &current_frames,
                                    frame_idx,
                                    connection_id,
                                )
                                .await
                                .is_err()
                                {
                                    break;
                                }
                                frame_sent_at = Some(Instant::now());
                            } else {
                                warn!("Frame rejetée après {} retransmissions, fermeture du WebSocket", MAX_FRAME_RETRIES);
                                break;
                            }
                        } else {
                            warn!("NACK inattendu ignoré");
                        }
                    }
                    Some(Ok(_)) => {}
                    Some(Err(error)) => {
                        info!("WebSocket fermé avec une erreur: {error}");
                        break;
                    }
                    None => break,
                }
            }

            else => break,
        }
    }

    // Nettoyage lors de la déconnexion
    info!("ESP32/Émulateur déconnecté du WebSocket.");
    let mut ctx = state.write().unwrap();
    if ctx.ws_connection_id == connection_id {
        ctx.ws_tx = None;
    }
}

async fn send_frame_at(
    socket: &mut WebSocket,
    frames: &[Vec<u8>],
    frame_idx: usize,
    connection_id: u64,
) -> Result<(), axum::Error> {
    let frame = frames
        .get(frame_idx)
        .expect("send_frame_at called with a frame outside the animation");
    info!(
        "WS envoi frame {}/{} (connexion {}, {} octets)",
        frame_idx + 1,
        frames.len(),
        connection_id,
        frame.len()
    );
    socket.send(Message::Binary(frame.clone().into())).await?;
    Ok(())
}

pub async fn process_for_esp32(
    State(SharedContext(state)): State<SharedContext>,
    request: Request,
) -> Response {
    let upload = match image_bytes(request).await {
        Ok(v) => v,
        Err(e) => return (StatusCode::BAD_REQUEST, e).into_response(),
    };

    let animation = match decode_image(&upload.bytes) {
        Ok(v) => v,
        Err(e) => return (StatusCode::BAD_REQUEST, e).into_response(),
    };

    let (matrix, options, sender) = {
        let context = state.read().unwrap();
        (
            context.matrix.clone(),
            context.options.clone(),
            context.ws_tx.clone(),
        )
    };

    let sender = match sender {
        Some(s) => s,
        None => {
            warn!("Tentative d'envoi d'image mais aucun WebSocket actif.");
            return (
                StatusCode::SERVICE_UNAVAILABLE,
                "Aucun ESP32 connecté en WebSocket",
            )
                .into_response();
        }
    };

    // Traitement des frames
    let processed_frames: Vec<Vec<u8>> = animation
        .frames
        .iter()
        .map(|frame| {
            let raw: Vec<u8> = frame.pixels.iter().flat_map(|p| [p.r, p.g, p.b]).collect();
            let transformed = transform(
                fit(&raw, frame.width, frame.height, &matrix, &options),
                &options,
                matrix.max_palette_colors,
            );
            transformed.to_raw_rgb888(matrix.enable_serpentine_layout)
        })
        .collect();

    info!(
        "Envoi ESP32: {} frame(s), matrice {}x{}, {} octets/frame",
        processed_frames.len(),
        matrix.width,
        matrix.height,
        processed_frames.first().map_or(0, Vec::len)
    );

    // Envoi non bloquant vers le canal Tokio
    if sender.send(processed_frames).await.is_err() {
        return (
            StatusCode::INTERNAL_SERVER_ERROR,
            "Impossible de transmettre l'image au WebSocket",
        )
            .into_response();
    }

    (StatusCode::OK, "ok !").into_response()
}
