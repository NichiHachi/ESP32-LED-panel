#pragma once

#include "../Types.hpp"

class ColorProcessor {
public:
    static void apply_corrections(Frame& frame, const ProcessingOptions& opts);
    static void apply_color_mode(Frame& frame, ColorMode mode, int max_colors);

private:
    static float color_distance(ColorFloat c1, ColorFloat c2);
    static void quantize_kmeans(Frame& frame, int k);
    static void convert_to_grayscale(Frame& frame);
    static void convert_to_rgb565(Frame& frame);
};