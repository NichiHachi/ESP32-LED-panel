#include "ColorModes.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>

float ColorProcessor::color_distance(const ColorFloat c1, const ColorFloat c2) {
    const float dr = c1.r - c2.r;
    const float dg = c1.g - c2.g;
    const float db = c1.b - c2.b;
    return std::sqrt(dr * dr + dg * dg + db * db);
}

void ColorProcessor::apply_corrections(Frame& frame, const ProcessingOptions& opts) {
    for (auto& [r, g, b] : frame.pixels) {
        float red = r / 255.0f;
        float green = g / 255.0f;
        float blue = b / 255.0f;

        red = (red - 0.5f) * opts.contrast + 0.5f + (opts.brightness - 1.0f);
        green = (green - 0.5f) * opts.contrast + 0.5f + (opts.brightness - 1.0f);
        blue = (blue - 0.5f) * opts.contrast + 0.5f + (opts.brightness - 1.0f);

        if (opts.gamma != 1.0f && opts.gamma > 0.0f) {
            red = std::pow(std::clamp(red, 0.0f, 1.0f), opts.gamma);
            green = std::pow(std::clamp(green, 0.0f, 1.0f), opts.gamma);
            blue = std::pow(std::clamp(blue, 0.0f, 1.0f), opts.gamma);
        }

        r = static_cast<uint8_t>(std::clamp(red * 255.0f, 0.0f, 255.0f));
        g = static_cast<uint8_t>(std::clamp(green * 255.0f, 0.0f, 255.0f));
        b = static_cast<uint8_t>(std::clamp(blue * 255.0f, 0.0f, 255.0f));
    }
}

void ColorProcessor::quantize_kmeans(Frame& frame, const int k) {
    if (frame.pixels.empty() || k <= 0) return;
    std::vector<ColorFloat> palette(k);

    for (int i = 0; i < k; ++i) {
        const int idx = std::rand() % frame.pixels.size();
        palette[i] = { .r = static_cast<float>(frame.pixels[idx].r), .g = static_cast<float>(frame.pixels[idx].g), .b = static_cast<float>(frame.pixels[idx].b) };
    }

    for (int iter = 0; iter < 10; ++iter) {
        std::vector<ColorFloat> new_means(k, {.r = 0, .g = 0, .b = 0});
        std::vector<int> counts(k, 0);

        for (const auto& [r, g, b] : frame.pixels) {
            const ColorFloat pixel = { .r = static_cast<float>(r), .g = static_cast<float>(g), .b = static_cast<float>(b) };
            int best_k = 0;
            float min_dist = color_distance(pixel, palette[0]);

            for (int j = 1; j < k; ++j) {
                const float dist = color_distance(pixel, palette[j]);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_k = j;
                }
            }

            new_means[best_k].r += pixel.r;
            new_means[best_k].g += pixel.g;
            new_means[best_k].b += pixel.b;
            counts[best_k]++;
        }

        for (int j = 0; j < k; ++j) {
            if (counts[j] > 0) {
                palette[j].r = new_means[j].r / counts[j];
                palette[j].g = new_means[j].g / counts[j];
                palette[j].b = new_means[j].b / counts[j];
            }
        }
    }

    for (auto& [r, g, b] : frame.pixels) {
        const ColorFloat pixel = { .r = static_cast<float>(r), .g = static_cast<float>(g), .b = static_cast<float>(b) };
        int best_k = 0;
        float min_dist = color_distance(pixel, palette[0]);

        for (size_t j = 1; j < palette.size(); ++j) {
            if (const float dist = color_distance(pixel, palette[j]); dist < min_dist) {
                min_dist = dist;
                best_k = j;
            }
        }

        r = static_cast<uint8_t>(std::clamp(palette[best_k].r, 0.0f, 255.0f));
        g = static_cast<uint8_t>(std::clamp(palette[best_k].g, 0.0f, 255.0f));
        b = static_cast<uint8_t>(std::clamp(palette[best_k].b, 0.0f, 255.0f));
    }
}

void ColorProcessor::convert_to_grayscale(Frame& frame) {
    for (auto& [r, g, b] : frame.pixels) {
        const auto gray = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
        r = g = b = gray;
    }
}

void ColorProcessor::convert_to_rgb565(Frame& frame) {
    for (auto& [r, g, b] : frame.pixels) {
        r = (r & 0xF8) | (r >> 5);
        g = (g & 0xFC) | (g >> 6);
        b = (b & 0xF8) | (b >> 5);
    }
}

void ColorProcessor::apply_color_mode(Frame& frame, const ColorMode mode, const int max_colors) {
    switch (mode) {
        case ColorMode::QUANTIZED_KMEANS:
            quantize_kmeans(frame, max_colors);
            break;
        case ColorMode::GRAYSCALE:
            convert_to_grayscale(frame);
            break;
        case ColorMode::RGB565:
            convert_to_rgb565(frame);
            break;
    }
}