use axum::{
    extract::{
        ws::{Message, WebSocket, WebSocketUpgrade},
        Request, State,
    },
    http::StatusCode,
    response::{IntoResponse, Response},
};
use std::sync::{Arc, RwLock};
use tokio::sync::mpsc;
use tracing::{info, warn};

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
    {
        let mut ctx = state.write().unwrap();
        ctx.ws_tx = Some(tx);
    }

    let mut current_frames: Vec<Vec<u8>> = Vec::new();
    let mut next_frame_idx: usize = 0;

    loop {
        tokio::select! {
            // Réception d'une nouvelle animation envoyée par `process_for_esp32`
            Some(new_frames) = rx.recv() => {
                info!("Nouvelles frames reçues dans la boucle WS ({})", new_frames.len());
                current_frames = new_frames;
                next_frame_idx = 0;

                // Envoi immédiat des 3 premières frames
                let initial_burst = current_frames.len().min(3);
                for _ in 0..initial_burst {
                    if let Some(frame_bytes) = current_frames.get(next_frame_idx) {
                        if socket.send(Message::Binary(frame_bytes.clone().into())).await.is_err() {
                            break;
                        }
                        next_frame_idx += 1;
                    }
                }
            }

            // Réception du "ACK" depuis le Python/ESP32
            Some(Ok(msg)) = socket.recv() => {
                if let Message::Text(text) = msg {
                    if text == "ACK" && !current_frames.is_empty() {
                        if next_frame_idx >= current_frames.len() {
                            next_frame_idx = 0; // Boucle sur l'animation
                        }
                        if let Some(frame_bytes) = current_frames.get(next_frame_idx) {
                            if socket.send(Message::Binary(frame_bytes.clone().into())).await.is_err() {
                                break;
                            }
                            next_frame_idx += 1;
                        }
                    }
                }
            }

            else => break,
        }
    }

    // Nettoyage lors de la déconnexion
    info!("ESP32/Émulateur déconnecté du WebSocket.");
    let mut ctx = state.write().unwrap();
    ctx.ws_tx = None;
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