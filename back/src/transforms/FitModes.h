#pragma once

#include "../Types.hpp"

class FitProcessor {
public:
    static Frame apply(const unsigned char* raw_data, int src_w, int src_h, const LedMatrixConfig& config, const ProcessingOptions& opts);

private:
    static Frame grid_downsample(const unsigned char* raw_img, int src_w, int src_h, int target_w, int target_h);
    static Frame apply_stretch(const unsigned char* raw_data, int src_w, int src_h, int target_w, int target_h);
    static Frame apply_crop(const unsigned char* raw_data, int src_w, int src_h, int target_w, int target_h);
    static Frame apply_letterbox(const unsigned char* raw_data, int src_w, int src_h, int target_w, int target_h, RGB bg_color);
};