use esp_led_manager::{config, esp, models, routes};
use tracing::info;

#[tokio::main]
async fn main() {
    use axum::{
        routing::{get, post},
        Router,
    };
    use std::{
        net::SocketAddr,
        sync::{Arc, RwLock},
    };

    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| "esp_led_manager=info,tower_http=info".into()),
        )
        .init();

    let state = models::SharedContext(Arc::new(RwLock::new(config::load_config("config.json"))));
    info!("Configuration chargée depuis config.json");
    let app = Router::new()
        .route("/", post(routes::process))
        .route(
            "/config",
            get(routes::get_config).post(routes::update_config),
        )
        .route("/esp", post(esp::process_for_esp32))
        .route("/esp/ws", get(esp::ws_handler))
        .with_state(state);
    let listener = tokio::net::TcpListener::bind(SocketAddr::from(([0, 0, 0, 0], 18080)))
        .await
        .unwrap();
    info!("Serveur démarré sur {}", listener.local_addr().unwrap());
    axum::serve(listener, app).await.unwrap();
}