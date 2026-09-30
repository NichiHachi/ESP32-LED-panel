use axum::{
    body::to_bytes,
    extract::{Query, Request, State},
    http::{header, StatusCode},
    response::{IntoResponse, Response},
    Json,
};
use base64::{engine::general_purpose::STANDARD as BASE64, Engine};
use futures_util::stream;
use serde_json::{json, Value};
use tracing::{debug, error, info, warn};

use crate::{
    config::{apply_update, save_config, MAX_UPLOAD_SIZE},
    media::{decode_image, encode_png, encode_zip},
    models::{Frame, OutputQuery, SharedContext},
    processing::{fit, transform},
};

fn enabled(value: Option<&str>) -> bool {
    matches!(value, Some("true" | "1"))
}

fn error_response(status: StatusCode, message: impl Into<String>) -> Response {
    (status, message.into()).into_response()
}

pub(crate) struct Upload {
    pub(crate) bytes: Vec<u8>,
    pub(crate) return_matrix: bool,
    pub(crate) return_zip: bool,
}

pub(crate) async fn image_bytes(request: Request) -> Result<Upload, String> {
    let headers = request.headers().clone();
    let body = to_bytes(request.into_body(), MAX_UPLOAD_SIZE)
        .await
        .map_err(|e| e.to_string())?;
    if body.is_empty() {
        warn!("Requête de traitement reçue sans contenu");
        return Err("Aucun fichier (image ou ZIP) reçu.".into());
    }
    if headers
        .get(header::CONTENT_TYPE)
        .and_then(|v| v.to_str().ok())
        .unwrap_or("")
        .starts_with("multipart/form-data")
    {
        let content_type = headers
            .get(header::CONTENT_TYPE)
            .unwrap()
            .to_str()
            .map_err(|e| e.to_string())?;
        debug!("Réception d'un upload multipart de {} octets", body.len());
        let boundary = multer::parse_boundary(content_type).map_err(|e| e.to_string())?;
        let mut multipart = multer::Multipart::new(
            stream::once(async move { Ok::<_, std::convert::Infallible>(body) }),
            boundary,
        );
        let mut image = None;
        let mut return_matrix = false;
        let mut return_zip = false;
        while let Some(field) = multipart.next_field().await.map_err(|e| e.to_string())? {
            let name = field.name().map(str::to_owned);
            let is_file = name.as_deref() == Some("image") || field.file_name().is_some();
            let data = field.bytes().await.map_err(|e| e.to_string())?;
            if is_file {
                image = Some(data.to_vec());
            } else if name.as_deref() == Some("return_matrix") {
                return_matrix = data.as_ref() == b"true" || data.as_ref() == b"1";
            } else if name.as_deref() == Some("return_zip") {
                return_zip = data.as_ref() == b"true" || data.as_ref() == b"1";
            }
        }
        return image
            .map(|bytes| Upload {
                bytes,
                return_matrix,
                return_zip,
            })
            .ok_or_else(|| "Aucun fichier (image ou ZIP) reçu.".into());
    }
    Ok(Upload {
        bytes: body.to_vec(),
        return_matrix: false,
        return_zip: false,
    })
}

pub async fn process(
    State(SharedContext(state)): State<SharedContext>,
    Query(query): Query<OutputQuery>,
    request: Request,
) -> Response {
    let return_matrix = enabled(query.return_matrix.as_deref());
    let return_zip = enabled(query.return_zip.as_deref());
    info!(return_matrix, return_zip, "Début du traitement d'une image");
    let upload = match image_bytes(request).await {
        Ok(v) => v,
        Err(e) => {
            warn!(error = %e, "Upload invalide");
            return error_response(StatusCode::BAD_REQUEST, e);
        }
    };
    debug!(
        bytes = upload.bytes.len(),
        multipart_return_matrix = upload.return_matrix,
        multipart_return_zip = upload.return_zip,
        "Upload reçu"
    );
    let animation = match decode_image(&upload.bytes) {
        Ok(v) => v,
        Err(e) => {
            warn!(error = %e, "Image impossible à décoder");
            return error_response(StatusCode::BAD_REQUEST, e);
        }
    };
    info!(
        frames = animation.frames.len(),
        animated = animation.is_animated,
        "Image décodée"
    );
    let (matrix, options) = {
        let context = state.read().unwrap();
        (context.matrix.clone(), context.options.clone())
    };
    let frames: Vec<Frame> = animation
        .frames
        .iter()
        .map(|frame| {
            let raw: Vec<u8> = frame.pixels.iter().flat_map(|p| [p.r, p.g, p.b]).collect();
            let mut output = transform(
                fit(&raw, frame.width, frame.height, &matrix, &options),
                &options,
                matrix.max_palette_colors,
            );
            output.duration_ms = frame.duration_ms;
            output
        })
        .collect();
    let serpentine = matrix.enable_serpentine_layout;
    if return_zip || upload.return_zip {
        info!("Encodage du résultat au format ZIP");
        return match encode_zip(&frames, serpentine) {
            Ok(data) => (
                [
                    (header::CONTENT_TYPE, "application/zip"),
                    (
                        header::CONTENT_DISPOSITION,
                        "attachment; filename=\"processed_frames.zip\"",
                    ),
                ],
                data,
            )
                .into_response(),
            Err(e) => {
                error!(error = %e, "Échec de l'encodage ZIP");
                error_response(StatusCode::INTERNAL_SERVER_ERROR, e)
            }
        };
    }
    if return_matrix || upload.return_matrix {
        info!("Retour de la matrice au format JSON");
        return Json(json!({
            "is_animated": animation.is_animated,
            "frame_count": frames.len(),
            "frames": frames.iter().map(|f| json!({
                "width": f.width,
                "height": f.height,
                "delay_ms": f.duration_ms,
                "pixels": f.pixels
            })).collect::<Vec<_>>()
        }))
            .into_response();
    }
    if !animation.is_animated {
        return match encode_png(&frames[0], serpentine) {
            Ok(data) => ([(header::CONTENT_TYPE, "image/png")], data).into_response(),
            Err(e) => {
                error!(error = %e, "Échec de l'encodage PNG");
                error_response(StatusCode::INTERNAL_SERVER_ERROR, e)
            }
        };
    }
    let mut encoded = Vec::with_capacity(frames.len());
    for (i, frame) in frames.iter().enumerate() {
        let png = match encode_png(frame, false) {
            Ok(data) => data,
            Err(message) => {
                error!(error = %message, frame = i, "Échec de l'encodage d'une frame PNG");
                return error_response(StatusCode::INTERNAL_SERVER_ERROR, message);
            }
        };
        encoded.push(json!({
            "index": i,
            "delay_ms": frame.duration_ms,
            "png_base64": BASE64.encode(png)
        }));
    }
    Json(json!({
        "is_animated": true,
        "frame_count": frames.len(),
        "frames": encoded
    }))
        .into_response()
}

pub async fn get_config(State(SharedContext(state)): State<SharedContext>) -> impl IntoResponse {
    debug!("Lecture de la configuration");
    let context = state.read().unwrap();
    Json(json!({"matrix":context.matrix,"options":context.options}))
}

pub async fn update_config(
    State(SharedContext(state)): State<SharedContext>,
    Json(value): Json<Value>,
) -> Response {
    info!("Demande de mise à jour de la configuration");
    let mut context = state.write().unwrap();
    let mut matrix = context.matrix.clone();
    let mut options = context.options.clone();
    if let Err(message) = apply_update(&value, &mut matrix, &mut options) {
        warn!(error = %message, "Mise à jour de configuration rejetée");
        return error_response(
            StatusCode::BAD_REQUEST,
            format!("Configuration invalide : {message}"),
        );
    }
    context.matrix = matrix;
    context.options = options;
    save_config(&context);
    info!("Configuration mise à jour et sauvegardée");
    (StatusCode::OK, "Configuration mise à jour avec succès.").into_response()
}