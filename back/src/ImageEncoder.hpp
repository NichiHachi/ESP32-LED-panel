#pragma once

#include "Types.hpp"
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "miniz.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

class ImageEncoder {
public:
    static std::vector<uint8_t> encode_to_png(const Frame &frame, bool is_serpentine = false) {
        std::vector<uint8_t> buffer;

        auto write_func = [](void *context, void *data, int size) {
            auto *vec = static_cast<std::vector<uint8_t> *>(context);
            const auto *bytes = static_cast<const uint8_t *>(data);
            vec->insert(vec->end(), bytes, bytes + size);
        };

        std::vector<uint8_t> rgb_bytes;
        rgb_bytes.reserve(frame.width * frame.height * 3);

        for (uint16_t y = 0; y < frame.height; ++y) {
            bool reverse_line = is_serpentine && (y % 2 != 0);
            for (uint16_t x = 0; x < frame.width; ++x) {
                uint16_t actual_x = reverse_line ? (frame.width - 1 - x) : x;
                size_t idx = static_cast<size_t>(y) * frame.width + actual_x;

                if (idx < frame.pixels.size()) {
                    const auto &p = frame.pixels[idx];
                    rgb_bytes.push_back(p.r);
                    rgb_bytes.push_back(p.g);
                    rgb_bytes.push_back(p.b);
                } else {
                    rgb_bytes.push_back(0);
                    rgb_bytes.push_back(0);
                    rgb_bytes.push_back(0);
                }
            }
        }

        stbi_write_png_to_func(write_func, &buffer, frame.width, frame.height, 3, rgb_bytes.data(), frame.width * 3);
        return buffer;
    }

    static std::vector<uint8_t> encode_frames_to_zip(const std::vector<Frame> &frames, bool is_serpentine = false) {
        mz_zip_archive zip;
        std::memset(&zip, 0, sizeof(zip));

        if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
            return {};
        }

        for (size_t i = 0; i < frames.size(); ++i) {
            std::vector<uint8_t> png_bytes = encode_to_png(frames[i], is_serpentine);

            char filename[32];
            std::snprintf(filename, sizeof(filename), "frame_%03zu.png", i);

            if (!mz_zip_writer_add_mem(&zip, filename, png_bytes.data(), png_bytes.size(), MZ_BEST_COMPRESSION)) {
                mz_zip_writer_end(&zip);
                return {};
            }
        }

        void *zip_buf = nullptr;
        size_t zip_size = 0;
        if (!mz_zip_writer_finalize_heap_archive(&zip, &zip_buf, &zip_size)) {
            mz_zip_writer_end(&zip);
            return {};
        }

        std::vector<uint8_t> output(
            static_cast<const uint8_t *>(zip_buf),
            static_cast<const uint8_t *>(zip_buf) + zip_size
        );

        mz_zip_writer_end(&zip);
        return output;
    }
};
