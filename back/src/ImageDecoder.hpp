#pragma once

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <vector>
#include <cstdint>

struct RawImageData {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    int channels = 0;
    bool valid = false;
};

class ImageDecoder {
public:
    static RawImageData decode_from_memory(const std::string_view bytes) {
        RawImageData img;
        unsigned char* data = stbi_load_from_memory(
            reinterpret_cast<const uint8_t*>(bytes.data()),
            static_cast<int>(bytes.size()),
            &img.width, &img.height, &img.channels, 3 // Forcer en RGB (3 canaux)
        );

        if (data) {
            img.pixels.assign(data, data + (img.width * img.height * 3));
            img.valid = true;
            stbi_image_free(data);
        }

        return img;
    }
};