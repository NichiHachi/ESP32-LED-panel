#include "FitModes.h"
#include <cmath>
#include <algorithm>

Frame FitProcessor::grid_downsample(const unsigned char* raw_img, const int src_w, const int src_h, const int target_w, const int target_h) {
    Frame out;
    out.width = target_w;
    out.height = target_h;
    out.pixels.resize(target_w * target_h);

    const float cell_w = static_cast<float>(src_w) / static_cast<float>(target_w);
    const float cell_h = static_cast<float>(src_h) / static_cast<float>(target_h);

    for (int ty = 0; ty < target_h; ++ty) {
        for (int tx = 0; tx < target_w; ++tx) {
            const int start_x = static_cast<int>(tx * cell_w);
            const int end_x   = std::min(src_w, static_cast<int>((tx + 1) * cell_w));
            const int start_y = static_cast<int>(ty * cell_h);
            const int end_y   = std::min(src_h, static_cast<int>((ty + 1) * cell_h));

            double sum_r = 0, sum_g = 0, sum_b = 0;
            int count = 0;

            for (int sy = start_y; sy < end_y; ++sy) {
                for (int sx = start_x; sx < end_x; ++sx) {
                    const int idx = (sy * src_w + sx) * 3;
                    sum_r += raw_img[idx];
                    sum_g += raw_img[idx + 1];
                    sum_b += raw_img[idx + 2];
                    count++;
                }
            }

            if (count > 0) {
                out.pixels[ty * target_w + tx] = {
                    .r = static_cast<uint8_t>(sum_r / count),
                    .g = static_cast<uint8_t>(sum_g / count),
                    .b = static_cast<uint8_t>(sum_b / count)
                };
            }
        }
    }
    return out;
}

Frame FitProcessor::apply_stretch(const unsigned char* raw_data, const int src_w, const int src_h, const int target_w, const int target_h) {
    return grid_downsample(raw_data, src_w, src_h, target_w, target_h);
}

Frame FitProcessor::apply_crop(const unsigned char* raw_data, const int src_w, const int src_h, const int target_w, const int target_h) {
    const float scale_x = static_cast<float>(target_w) / static_cast<float>(src_w);
    const float scale_y = static_cast<float>(target_h) / static_cast<float>(src_h);
    const float scale = std::max(scale_x, scale_y);

    const int scaled_w = static_cast<int>(src_w * scale);
    const int scaled_h = static_cast<int>(src_h * scale);

    const Frame scaled = grid_downsample(raw_data, src_w, src_h, scaled_w, scaled_h);

    Frame final_frame;
    final_frame.width = target_w;
    final_frame.height = target_h;
    final_frame.pixels.resize(target_w * target_h);

    const int crop_x = (scaled_w - target_w) / 2;
    const int crop_y = (scaled_h - target_h) / 2;

    for (int y = 0; y < target_h; ++y) {
        for (int x = 0; x < target_w; ++x) {
            final_frame.pixels[y * target_w + x] = scaled.pixels[(y + crop_y) * scaled_w + (x + crop_x)];
        }
    }
    return final_frame;
}

Frame FitProcessor::apply_letterbox(const unsigned char* raw_data, const int src_w, const int src_h, const int target_w, const int target_h, const RGB bg_color) {
    const float scale_x = static_cast<float>(target_w) / static_cast<float>(src_w);
    const float scale_y = static_cast<float>(target_h) / static_cast<float>(src_h);
    const float scale = std::min(scale_x, scale_y);

    const int scaled_w = std::max(1, static_cast<int>(src_w * scale));
    const int scaled_h = std::max(1, static_cast<int>(src_h * scale));

    const Frame scaled_frame = grid_downsample(raw_data, src_w, src_h, scaled_w, scaled_h);

    Frame final_frame;
    final_frame.width = target_w;
    final_frame.height = target_h;
    final_frame.pixels.resize(target_w * target_h, bg_color);

    const int offset_x = (target_w - scaled_w) / 2;
    const int offset_y = (target_h - scaled_h) / 2;

    for (int y = 0; y < scaled_h; ++y) {
        for (int x = 0; x < scaled_w; ++x) {
            const int dst_idx = (y + offset_y) * target_w + (x + offset_x);
            const int src_idx = y * scaled_w + x;
            final_frame.pixels[dst_idx] = scaled_frame.pixels[src_idx];
        }
    }
    return final_frame;
}

Frame FitProcessor::apply(const unsigned char* raw_data, const int src_w, const int src_h, const LedMatrixConfig& config, const ProcessingOptions& opts) {
    switch (opts.fit_mode) {
        case FitMode::STRETCH:
            return apply_stretch(raw_data, src_w, src_h, config.width, config.height);
        case FitMode::CROP:
            return apply_crop(raw_data, src_w, src_h, config.width, config.height);
        case FitMode::LETTERBOX:
        default:
            return apply_letterbox(raw_data, src_w, src_h, config.width, config.height, opts.background_color);
    }
}