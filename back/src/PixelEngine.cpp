#include "PixelEngine.h"
#include "transforms/FitModes.h"
#include "transforms/ColorModes.h"
#include "transforms/GeometryTransforms.h"

PixelEngine::PixelEngine(const LedMatrixConfig &cfg) : config(cfg) {}

Frame PixelEngine::process_raw_image(const unsigned char* raw_data, const int src_w, const int src_h, const ProcessingOptions& opts) const {
    Frame frame = FitProcessor::apply(raw_data, src_w, src_h, config, opts);

    ColorProcessor::apply_corrections(frame, opts);
    frame = GeometryProcessor::apply(frame, opts);
    ColorProcessor::apply_color_mode(frame, opts.color_mode, config.max_palette_colors);

    return frame;
}