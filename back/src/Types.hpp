#pragma once

#include <vector>
#include <cstdint>

struct RGB {
    uint8_t r = 0, g = 0, b = 0;
};

struct ColorFloat {
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

struct Frame {
    int width = 0;
    int height = 0;
    std::vector<RGB> pixels; // Taille: width * height
    uint32_t duration_ms = 100;
};

struct LedMatrixConfig {
    int width = 64;
    int height = 64;
    int max_palette_colors = 64;
    bool enable_serpentine_layout = false;
};

enum class FitMode {
    LETTERBOX,
    CROP,
    STRETCH
};

enum class ColorMode {
    QUANTIZED_KMEANS,
    RGB565,
    GRAYSCALE
};

struct ProcessingOptions {
    FitMode fit_mode = FitMode::LETTERBOX;
    ColorMode color_mode = ColorMode::QUANTIZED_KMEANS;
    RGB background_color = {.r = 0, .g = 0, .b = 0};

    float brightness = 1.0f;
    float contrast = 1.0f;
    float gamma = 1.0f;
    int rotation_deg = 0;
    bool flip_horizontal = false;
    bool flip_vertical = false;
};

using PixelMatrix = std::vector<std::vector<RGB> >;
