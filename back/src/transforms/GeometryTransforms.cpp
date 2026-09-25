#include "GeometryTransforms.h"

Frame GeometryProcessor::apply(const Frame& src, const ProcessingOptions& opts) {
    const int w = src.width;
    const int h = src.height;

    const int rot = ((opts.rotation_deg % 360) + 360) % 360;

    const int new_w = (rot == 90 || rot == 270) ? h : w;
    const int new_h = (rot == 90 || rot == 270) ? w : h;

    Frame dst;
    dst.width = new_w;
    dst.height = new_h;
    dst.pixels.resize(new_w * new_h);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int src_x = opts.flip_horizontal ? (w - 1 - x) : x;
            const int src_y = opts.flip_vertical ? (h - 1 - y) : y;

            int dst_x = src_x;
            int dst_y = src_y;

            if (rot == 90) {
                dst_x = h - 1 - src_y;
                dst_y = src_x;
            } else if (rot == 180) {
                dst_x = w - 1 - src_x;
                dst_y = h - 1 - src_y;
            } else if (rot == 270) {
                dst_x = src_y;
                dst_y = w - 1 - src_x;
            }

            dst.pixels[dst_y * new_w + dst_x] = src.pixels[src_y * w + src_x];
        }
    }

    return dst;
}