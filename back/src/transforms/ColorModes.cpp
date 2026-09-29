#include "transforms/ColorModes.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <dlib/clustering.h>

float ColorProcessor::color_distance(const ColorFloat &c1, const ColorFloat &c2) {
    const float dr = c1.r - c2.r;
    const float dg = c1.g - c2.g;
    const float db = c1.b - c2.b;
    return std::sqrt(dr * dr + dg * dg + db * db);
}

void ColorProcessor::apply_corrections(Frame &frame, const ProcessingOptions &opts) {
    for (auto &[r, g, b]: frame.pixels) {
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

void ColorProcessor::quantize_kmeans(Frame &frame, const int k) {
    if (frame.pixels.empty() || k <= 0) return;
    const int effective_k = std::min(k, static_cast<int>(frame.pixels.size()));

    using sample_type = dlib::matrix<float, 3, 1>;
    std::vector<sample_type> samples;
    samples.reserve(frame.pixels.size());

    for (const auto &[r, g, b]: frame.pixels) {
        sample_type m;
        m(0) = static_cast<float>(r);
        m(1) = static_cast<float>(g);
        m(2) = static_cast<float>(b);
        samples.push_back(m);
    }

    std::vector<sample_type> initial_centers;
    dlib::pick_initial_centers(effective_k, initial_centers, samples);
    dlib::find_clusters_using_kmeans(samples, initial_centers);

    if (initial_centers.empty()) return;
    const int num_centers = static_cast<int>(initial_centers.size());

    // 3. Réattribution des couleurs
    for (size_t i = 0; i < frame.pixels.size(); ++i) {
        int best_k = 0;
        float min_dist = dlib::length_squared(samples[i] - initial_centers[0]);
        for (int j = 1; j < num_centers; ++j) {
            float dist = dlib::length_squared(samples[i] - initial_centers[j]);
            if (dist < min_dist) {
                min_dist = dist;
                best_k = j;
            }
        }
        frame.pixels[i].r = static_cast<uint8_t>(std::clamp(initial_centers[best_k](0), 0.0f, 255.0f));
        frame.pixels[i].g = static_cast<uint8_t>(std::clamp(initial_centers[best_k](1), 0.0f, 255.0f));
        frame.pixels[i].b = static_cast<uint8_t>(std::clamp(initial_centers[best_k](2), 0.0f, 255.0f));
    }
}

void ColorProcessor::convert_to_grayscale(Frame &frame) {
    for (auto &[r, g, b]: frame.pixels) {
        const auto gray = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
        r = g = b = gray;
    }
}

void ColorProcessor::convert_to_rgb565(Frame &frame) {
    for (auto &[r, g, b]: frame.pixels) {
        r = (r & 0xF8) | (r >> 5);
        g = (g & 0xFC) | (g >> 6);
        b = (b & 0xF8) | (b >> 5);
    }
}

void ColorProcessor::apply_color_mode(Frame &frame, const ColorMode mode, const int max_colors) {
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
