use esp_led_manager::{config, models, routes};

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

    let state = models::SharedContext(Arc::new(RwLock::new(config::load_config("config.json"))));
    let app = Router::new()
        .route("/", post(routes::process))
        .route(
            "/api/config",
            get(routes::get_config).post(routes::update_config),
        )
        .with_state(state);
    let listener = tokio::net::TcpListener::bind(SocketAddr::from(([0, 0, 0, 0], 18080)))
        .await
        .unwrap();
    axum::serve(listener, app).await.unwrap();
}