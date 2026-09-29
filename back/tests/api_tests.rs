#[cfg(test)]
mod tests {
    use axum::{
        body::Body,
        http::{header, Request, StatusCode},
        routing::{get, post},
        Router,
    };
    use serde_json::{json, Value};
    use std::{
        io::Write,
        sync::{Arc, RwLock},
    };
    use tower::ServiceExt; // Fournit la méthode .oneshot() pour tester l'application Axum

    // Import des modules du crate principal
    // (Ajustez les chemins si le nom de votre crate dans Cargo.toml est différent)
    use esp_led_manager::{
        models::{AppContext, SharedContext},
        routes,
    };

    /// Initialise une instance isolée de l'application Axum pour les tests d'intégration.
    fn setup_test_app() -> Router {
        let state = SharedContext(Arc::new(RwLock::new(AppContext::default())));
        Router::new()
            .route("/", post(routes::process))
            .route(
                "/api/config",
                get(routes::get_config).post(routes::update_config),
            )
            .with_state(state)
    }

    /// Génère un buffer PNG simple de dimensions spécifiées
    fn generate_dummy_png(width: u32, height: u32) -> Vec<u8> {
        use image::{ImageBuffer, ImageFormat, RgbImage};
        use std::io::Cursor;

        let img: RgbImage = ImageBuffer::from_fn(width, height, |x, y| {
            if (x + y) % 2 == 0 {
                image::Rgb([255, 0, 0])
            } else {
                image::Rgb([0, 0, 255])
            }
        });
        let mut cursor = Cursor::new(Vec::new());
        img.write_to(&mut cursor, ImageFormat::Png).unwrap();
        cursor.into_inner()
    }

    /// Génère un body multipart/form-data valide avec un fichier image
    fn generate_multipart_body(boundary: &str, field_name: &str, file_name: &str, data: &[u8]) -> Vec<u8> {
        let mut body = Vec::new();
        write!(
            body,
            "--{}\r\nContent-Disposition: form-data; name=\"{}\"; filename=\"{}\"\r\nContent-Type: image/png\r\n\r\n",
            boundary, field_name, file_name
        ).unwrap();
        body.extend_from_slice(data);
        write!(body, "\r\n--{}--\r\n", boundary).unwrap();
        body
    }

    // =========================================================================
    // 1. TESTS CONFORMITÉ README : GET & POST /api/config
    // =========================================================================

    #[tokio::test]
    async fn test_readme_get_config_default_structure() {
        let app = setup_test_app();

        let response = app
            .oneshot(
                Request::builder()
                    .method("GET")
                    .uri("/api/config")
                    .body(Body::empty())
                    .unwrap(),
            )
            .await
            .unwrap();

        assert_eq!(response.status(), StatusCode::OK);

        let body_bytes = axum::body::to_bytes(response.into_body(), usize::MAX)
            .await
            .unwrap();
        let json: Value = serde_json::from_slice(&body_bytes).unwrap();

        // Vérification de la présence des blocs documentés dans le README
        assert!(json.get("matrix").is_some());
        assert!(json.get("options").is_some());

        // Vérification des clés principales du sous-objet matrix
        let matrix = &json["matrix"];
        assert!(matrix.get("width").is_some());
        assert!(matrix.get("height").is_some());
        assert!(matrix.get("max_palette_colors").is_some());
        assert!(matrix.get("enable_serpentine_layout").is_some());

        // Vérification des clés principales du sous-objet options
        let options = &json["options"];
        assert!(options.get("fit_mode").is_some());
        assert!(options.get("color_mode").is_some());
        assert!(options.get("background_color").is_some());
        assert!(options.get("brightness").is_some());
        assert!(options.get("contrast").is_some());
        assert!(options.get("gamma").is_some());
        assert!(options.get("rotation_deg").is_some());
        assert!(options.get("flip_horizontal").is_some());
        assert!(options.get("flip_vertical").is_some());
    }

    #[tokio::test]
    async fn test_readme_post_config_partial_update() {
        let app = setup_test_app();

        // Exemple direct du README : Mise à jour partielle (luminosité, contraste, rotation, fit_mode)
        let payload = json!({
            "matrix": {
                "width": 32,
                "height": 32
            },
            "options": {
                "brightness": 1.2,
                "contrast": 1.1,
                "rotation_deg": 90,
                "fit_mode": "CROP"
            }
        });

        let response = app
            .clone()
            .oneshot(
                Request::builder()
                    .method("POST")
                    .uri("/api/config")
                    .header(header::CONTENT_TYPE, "application/json")
                    .body(Body::from(payload.to_string()))
                    .unwrap(),
            )
            .await
            .unwrap();

        assert_eq!(response.status(), StatusCode::OK);

        // Vérification que les modifications ont bien été appliquées
        let response_get = app
            .oneshot(
                Request::builder()
                    .method("GET")
                    .uri("/api/config")
                    .body(Body::empty())
                    .unwrap(),
            )
            .await
            .unwrap();

        let body_bytes = axum::body::to_bytes(response_get.into_body(), usize::MAX)
            .await
            .unwrap();
        let config: Value = serde_json::from_slice(&body_bytes).unwrap();

        assert_eq!(config["matrix"]["width"], 32);
        assert_eq!(config["matrix"]["height"], 32);
        assert_eq!(config["options"]["brightness"].as_f64().unwrap() as f32, 1.2_f32);
        assert_eq!(config["options"]["rotation_deg"], 90);
        assert_eq!(config["options"]["fit_mode"], "CROP");
    }

    #[tokio::test]
    async fn test_readme_post_config_invalid_rotation() {
        let app = setup_test_app();

        // Le README spécifie rotation_deg ∈ {0, 90, 180, 270}
        let payload = json!({
            "options": {
                "rotation_deg": 45
            }
        });

        let response = app
            .oneshot(
                Request::builder()
                    .method("POST")
                    .uri("/api/config")
                    .header(header::CONTENT_TYPE, "application/json")
                    .body(Body::from(payload.to_string()))
                    .unwrap(),
            )
            .await
            .unwrap();

        assert_eq!(response.status(), StatusCode::BAD_REQUEST);
    }

    // =========================================================================
    // 2. TESTS CONFORMITÉ README : POST / (Traitement d'Image)
    // =========================================================================

    #[tokio::test]
    async fn test_readme_process_image_raw_binary() {
        let app = setup_test_app();
        let png_data = generate_dummy_png(16, 16);

        // Option B du README : Corps binaire brut (Content-Type: application/octet-stream)
        let response = app
            .oneshot(
                Request::builder()
                    .method("POST")
                    .uri("/")
                    .header(header::CONTENT_TYPE, "application/octet-stream")
                    .body(Body::from(png_data))
                    .unwrap(),
            )
            .await
            .unwrap();

        assert_eq!(response.status(), StatusCode::OK);
        assert_eq!(
            response.headers().get(header::CONTENT_TYPE).unwrap(),
            "image/png"
        );
    }

    #[tokio::test]
    async fn test_readme_process_image_multipart() {
        let app = setup_test_app();
        let png_data = generate_dummy_png(16, 16);

        let boundary = "------------------------X1Y2Z3";
        let body = generate_multipart_body(boundary, "image", "test_image.png", &png_data);

        // Option A du README : Formulaire multipart/form-data
        let response = app
            .oneshot(
                Request::builder()
                    .method("POST")
                    .uri("/")
                    .header(
                        header::CONTENT_TYPE,
                        format!("multipart/form-data; boundary={}", boundary),
                    )
                    .body(Body::from(body))
                    .unwrap(),
            )
            .await
            .unwrap();

        assert_eq!(response.status(), StatusCode::OK);
        assert_eq!(
            response.headers().get(header::CONTENT_TYPE).unwrap(),
            "image/png"
        );
    }

    #[tokio::test]
    async fn test_readme_process_return_matrix_query_param() {
        let app = setup_test_app();
        let png_data = generate_dummy_png(8, 8);

        // Option 1 du README : Query parameter ?return_matrix=true
        let response = app
            .oneshot(
                Request::builder()
                    .method("POST")
                    .uri("/?return_matrix=true")
                    .header(header::CONTENT_TYPE, "application/octet-stream")
                    .body(Body::from(png_data))
                    .unwrap(),
            )
            .await
            .unwrap();

        assert_eq!(response.status(), StatusCode::OK);
        assert_eq!(
            response.headers().get(header::CONTENT_TYPE).unwrap(),
            "application/json"
        );

        let body_bytes = axum::body::to_bytes(response.into_body(), usize::MAX)
            .await
            .unwrap();
        let json: Value = serde_json::from_slice(&body_bytes).unwrap();

        // Le README indique que la réponse contient "frames" et "pixels"
        assert!(json.get("frames").is_some());
        let frames = json["frames"].as_array().unwrap();
        assert!(!frames.is_empty());
        assert!(frames[0].get("pixels").is_some());
    }

    #[tokio::test]
    async fn test_readme_process_return_zip_query_param() {
        let app = setup_test_app();
        let png_data = generate_dummy_png(8, 8);

        // Exécution spécifiée dans le README : ?return_zip=true
        let response = app
            .oneshot(
                Request::builder()
                    .method("POST")
                    .uri("/?return_zip=true")
                    .header(header::CONTENT_TYPE, "application/octet-stream")
                    .body(Body::from(png_data))
                    .unwrap(),
            )
            .await
            .unwrap();

        assert_eq!(response.status(), StatusCode::OK);
        assert_eq!(
            response.headers().get(header::CONTENT_TYPE).unwrap(),
            "application/zip"
        );
    }

    #[tokio::test]
    async fn test_readme_process_empty_body_error() {
        let app = setup_test_app();

        // Doit retourner 400 Bad Request si aucune image n'est transmise
        let response = app
            .oneshot(
                Request::builder()
                    .method("POST")
                    .uri("/")
                    .body(Body::empty())
                    .unwrap(),
            )
            .await
            .unwrap();

        assert_eq!(response.status(), StatusCode::BAD_REQUEST);
    }
}