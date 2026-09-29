use serde_json::{json, Value};
use std::fs;
use tracing::{error, warn};

use crate::models::{AppContext, MatrixConfig, Options, Rgb};

pub const MAX_UPLOAD_SIZE: usize = 32 * 1024 * 1024;
pub const MAX_UNCOMPRESSED_SIZE: u64 = 64 * 1024 * 1024;
pub const MAX_MATRIX_DIMENSION: i32 = 1024;
pub const MAX_PALETTE_SIZE: i32 = 256;

pub fn validate_config(matrix: &MatrixConfig, options: &Options) -> Result<(), String> {
    if !(1..=MAX_MATRIX_DIMENSION).contains(&matrix.width)
        || !(1..=MAX_MATRIX_DIMENSION).contains(&matrix.height)
    {
        return Err("matrix dimensions are out of range".into());
    }
    if !(1..=MAX_PALETTE_SIZE).contains(&matrix.max_palette_colors) {
        return Err("max_palette_colors is out of range".into());
    }
    if !options.brightness.is_finite()
        || options.brightness < 0.0
        || !options.contrast.is_finite()
        || options.contrast < 0.0
        || !options.gamma.is_finite()
        || options.gamma <= 0.0
    {
        return Err("color correction values are invalid".into());
    }
    if options.rotation_deg % 90 != 0 {
        return Err("rotation_deg must be a multiple of 90".into());
    }
    Ok(())
}

pub fn load_config(path: &str) -> AppContext {
    let mut context = AppContext {
        config_path: path.into(),
        ..Default::default()
    };
    if let Ok(contents) = fs::read_to_string(path) {
        if let Ok(value) = serde_json::from_str::<Value>(&contents) {
            if let Err(message) = apply_update(&value, &mut context.matrix, &mut context.options) {
                warn!(path, error = %message, "Configuration invalide, valeurs par défaut conservées");
            }
        } else {
            warn!(
                path,
                "Configuration JSON invalide, valeurs par défaut conservées"
            );
        }
    } else {
        warn!(
            path,
            "Fichier de configuration absent, création des valeurs par défaut"
        );
        save_config(&context);
    }
    context
}

pub fn save_config(context: &AppContext) {
    let value = json!({ "matrix": context.matrix, "options": context.options });
    if let Err(error) = fs::write(
        &context.config_path,
        serde_json::to_string_pretty(&value).unwrap(),
    ) {
        error!(path = %context.config_path, %error, "Impossible de sauvegarder la configuration");
    }
}

pub fn apply_update(
    value: &Value,
    matrix: &mut MatrixConfig,
    options: &mut Options,
) -> Result<(), String> {
    let matrix_value = value.get("matrix").unwrap_or(value);
    let options_value = value.get("options").unwrap_or(value);
    if !matrix_value.is_object() || !options_value.is_object() {
        return Err("matrix/options must be objects".into());
    }
    macro_rules! set {
        ($object:expr, $target:expr, $field:ident, $ty:ty) => {
            if let Some(value) = $object.get(stringify!($field)) {
                $target = serde_json::from_value::<$ty>(value.clone())
                    .map_err(|_| format!("invalid {}", stringify!($field)))?;
            }
        };
    }
    set!(matrix_value, matrix.width, width, i32);
    set!(matrix_value, matrix.height, height, i32);
    set!(
        matrix_value,
        matrix.max_palette_colors,
        max_palette_colors,
        i32
    );
    set!(
        matrix_value,
        matrix.enable_serpentine_layout,
        enable_serpentine_layout,
        bool
    );
    set!(options_value, options.brightness, brightness, f32);
    set!(options_value, options.contrast, contrast, f32);
    set!(options_value, options.gamma, gamma, f32);
    set!(options_value, options.rotation_deg, rotation_deg, i32);
    set!(
        options_value,
        options.flip_horizontal,
        flip_horizontal,
        bool
    );
    set!(options_value, options.flip_vertical, flip_vertical, bool);
    if let Some(value) = options_value.get("fit_mode") {
        options.fit_mode = parse_mode(value, "fit_mode")?;
    }
    if let Some(value) = options_value.get("color_mode") {
        options.color_mode = parse_mode(value, "color_mode")?;
    }
    if let Some(value) = options_value.get("background_color") {
        let values: Vec<u8> = serde_json::from_value(value.clone())
            .map_err(|_| "invalid background_color".to_string())?;
        if values.len() != 3 {
            return Err("background_color must contain 3 values".into());
        }
        options.background_color = Rgb {
            r: values[0],
            g: values[1],
            b: values[2],
        };
    }
    validate_config(matrix, options)
}

fn parse_mode<T: for<'de> serde::Deserialize<'de>>(value: &Value, name: &str) -> Result<T, String> {
    serde_json::from_value(value.clone()).map_err(|_| format!("invalid {name}"))
}
