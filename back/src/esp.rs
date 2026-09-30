use axum::{
    extract::{Request, State},
    http::{StatusCode},
    response::{IntoResponse, Response},
};

use crate::{
    media::decode_image,
    models::{Frame, SharedContext},
    processing::{fit, transform},
    routes::image_bytes,
};

/// Handlers dédiés à l'ESP32 : renvoie du binaire brut (application/octet-stream)
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

    let (matrix, options, esp_url) = {
        let context = state.read().unwrap();
        (context.matrix.clone(), context.options.clone(), context.esp_url.clone())
    };

    // Transformation des frames au format cible (dimension matrix.width x matrix.height)
    let processed_frames: Vec<Frame> = animation
        .frames
        .iter()
        .map(|frame| {
            let raw: Vec<u8> = frame.pixels.iter().flat_map(|p| [p.r, p.g, p.b]).collect();
            transform(
                fit(&raw, frame.width, frame.height, &matrix, &options),
                &options,
                matrix.max_palette_colors,
            )
        })
        .collect();

    let mut payload = Vec::new();
    for frame in &processed_frames {
        payload.extend(frame.to_raw_rgb888(matrix.enable_serpentine_layout));
    }

    // Envoi du binaire à l'ESP32
    let client = reqwest::Client::new();
    let res = client
        .post(&esp_url)
        .header("Content-Type", "application/octet-stream")
        .header("x-frame-count", processed_frames.len().to_string())
        .body(payload)
        .send()
        .await;

    // 4. Réponse au demandeur
    match res {
        Ok(response) if response.status().is_success() => {
            (StatusCode::OK, "ok !").into_response()
        }
        Ok(response) => (
            StatusCode::BAD_GATEWAY,
            format!("Erreur retournée par l'ESP32: {}", response.status()),
        )
            .into_response(),
        Err(err) => (
            StatusCode::GATEWAY_TIMEOUT,
            format!("Impossible de joindre l'ESP32 : {err}"),
        )
            .into_response(),
    }
}