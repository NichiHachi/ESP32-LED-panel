#pragma once

#include "Types.hpp"
#include <vector>
#include <cstdint>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

class ImageEncoder {
public:
    static std::vector<uint8_t> encode_to_png(const Frame& frame) {
        std::vector<uint8_t> buffer;

        auto write_func = [](void* context, void* data, int size) {
            auto* vec = static_cast<std::vector<uint8_t>*>(context);
            const auto* bytes = static_cast<const uint8_t*>(data);
            vec->insert(vec->end(), bytes, bytes + size);
        };

        std::vector<uint8_t> rgb_bytes;
        rgb_bytes.reserve(frame.pixels.size() * 3);
        for (const auto& p : frame.pixels) {
            rgb_bytes.push_back(p.r);
            rgb_bytes.push_back(p.g);
            rgb_bytes.push_back(p.b);
        }

        stbi_write_png_to_func(write_func, &buffer, frame.width, frame.height, 3, rgb_bytes.data(), frame.width * 3);
        return buffer;
    }
};