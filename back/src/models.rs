use serde::{Deserialize, Serialize};
use std::sync::{Arc, RwLock};

#[derive(Clone, Copy, Serialize, Deserialize, Default)]
pub struct Rgb {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

#[derive(Clone, Serialize, Deserialize)]
pub struct Frame {
    pub width: usize,
    pub height: usize,
    pub pixels: Vec<Rgb>,
    #[serde(skip)]
    pub duration_ms: u32,
}

#[derive(Clone, Serialize, Deserialize)]
pub struct MatrixConfig {
    pub width: i32,
    pub height: i32,
    pub max_palette_colors: i32,
    pub enable_serpentine_layout: bool,
}

#[derive(Clone, Serialize, Deserialize)]
pub enum FitMode {
    #[serde(rename = "LETTERBOX")]
    Letterbox,
    #[serde(rename = "CROP")]
    Crop,
    #[serde(rename = "STRETCH")]
    Stretch,
}

impl Default for FitMode {
    fn default() -> Self {
        Self::Letterbox
    }
}

#[derive(Clone, Serialize, Deserialize)]
pub enum ColorMode {
    #[serde(rename = "QUANTIZED_KMEANS")]
    QuantizedKmeans,
    #[serde(rename = "RGB565")]
    Rgb565,
    #[serde(rename = "GRAYSCALE")]
    Grayscale,
}

impl Default for ColorMode {
    fn default() -> Self {
        Self::QuantizedKmeans
    }
}

#[derive(Clone, Serialize, Deserialize)]
pub struct Options {
    pub fit_mode: FitMode,
    pub color_mode: ColorMode,
    pub background_color: Rgb,
    pub brightness: f32,
    pub contrast: f32,
    pub gamma: f32,
    pub rotation_deg: i32,
    pub flip_horizontal: bool,
    pub flip_vertical: bool,
}

impl Default for Options {
    fn default() -> Self {
        Self {
            fit_mode: FitMode::default(),
            color_mode: ColorMode::default(),
            background_color: Rgb::default(),
            brightness: 1.0,
            contrast: 1.0,
            gamma: 1.0,
            rotation_deg: 0,
            flip_horizontal: false,
            flip_vertical: false,
        }
    }
}

#[derive(Clone)]
pub struct AppContext {
    pub config_path: String,
    pub matrix: MatrixConfig,
    pub options: Options,
    pub ws_tx: Option<tokio::sync::mpsc::Sender<Vec<Vec<u8>>>>,
}

impl Default for AppContext {
    fn default() -> Self {
        Self {
            config_path: "config.json".into(),
            matrix: MatrixConfig {
                width: 64,
                height: 64,
                max_palette_colors: 64,
                enable_serpentine_layout: false,
            },
            options: Options::default(),
            ws_tx: None,
        }
    }
}

#[derive(Clone)]
pub struct SharedContext(pub Arc<RwLock<AppContext>>);

#[derive(Deserialize, Default)]
pub struct OutputQuery {
    pub return_matrix: Option<String>,
    pub return_zip: Option<String>,
}

#[derive(Clone)]
pub struct Animation {
    pub frames: Vec<Frame>,
    pub is_animated: bool,
}
