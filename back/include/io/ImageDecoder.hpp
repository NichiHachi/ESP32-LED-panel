#pragma once

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <vector>
#include <cstdint>
#include <string_view>
#include <cstring>
#include <algorithm>
#include "miniz.h"

constexpr size_t max_uncompressed_size = 64 * 1024 * 1024;

struct FrameData {
    std::vector<uint8_t> pixels;
    uint16_t width{0};
    uint16_t height{0};
    int delay_ms{100};
};

struct AnimationData {
    std::vector<FrameData> frames;
    bool is_animated{false};
    bool valid{false};
};

struct RawImageData {
    std::vector<uint8_t> pixels; // Format RGB
    int width = 0;
    int height = 0;
    int channels = 0;
    bool valid = false;
};

class ImageDecoder {
public:
    static AnimationData decode_zip_from_memory(std::string_view bytes) {
        AnimationData anim;
        mz_zip_archive zip_archive;
        std::memset(&zip_archive, 0, sizeof(zip_archive));

        if (!mz_zip_reader_init_mem(&zip_archive, bytes.data(), bytes.size(), 0)) {
            return anim;
        }

        mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
        size_t total_uncompressed_size = 0;

        for (mz_uint i = 0; i < num_files; ++i) {
            mz_zip_archive_file_stat file_stat;
            if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
                continue;
            }

            if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
                continue;
            }

            size_t uncompressed_size = file_stat.m_uncomp_size;
            if (uncompressed_size == 0 || total_uncompressed_size + uncompressed_size > max_uncompressed_size) {
                continue;
            }

            total_uncompressed_size += uncompressed_size;

            if (std::vector<uint8_t> buffer(uncompressed_size); mz_zip_reader_extract_to_mem(&zip_archive, i, buffer.data(), uncompressed_size, 0)) {
                std::string_view file_bytes(reinterpret_cast<const char *>(buffer.data()), buffer.size());

                if (AnimationData sub_anim = decode_animation_from_memory(file_bytes); sub_anim.valid) {
                    for (auto &frame: sub_anim.frames) {
                        anim.frames.push_back(std::move(frame));
                    }
                }
            }
        }

        mz_zip_reader_end(&zip_archive);

        if (!anim.frames.empty()) {
            anim.valid = true;
            anim.is_animated = (anim.frames.size() > 1);
        }

        return anim;
    }

    static AnimationData decode_animation_from_memory(std::string_view bytes) {
        AnimationData anim;

        int w = 0, h = 0, comp = 0;
        int *delays = nullptr;
        int frame_count = 0;

        // 1. Décodage du GIF animé en RGBA (4 canaux pour capturer la transparence)
        stbi_uc *data = stbi_load_gif_from_memory(
            reinterpret_cast<const stbi_uc *>(bytes.data()),
            static_cast<int>(bytes.size()),
            &delays, &w, &h, &frame_count, &comp, 4
        );

        if (data && frame_count > 0) {
            if (w <= 0 || h <= 0 || w > UINT16_MAX || h > UINT16_MAX) {
                if (delays) STBI_FREE(delays);
                STBI_FREE(data);
                return anim;
            }
            anim.valid = true;
            anim.is_animated = (frame_count > 1);

            size_t pixel_count = static_cast<size_t>(w * h);

            for (int i = 0; i < frame_count; ++i) {
                FrameData frame;
                frame.width = static_cast<uint16_t>(w);
                frame.height = static_cast<uint16_t>(h);
                frame.delay_ms = delays ? delays[i] : 100;

                const uint8_t *frame_ptr = data + (i * pixel_count * 4);
                frame.pixels.reserve(pixel_count * 3);

                // Conversion RGBA -> RGB avec composition de la transparence sur fond noir
                for (size_t p = 0; p < pixel_count; ++p) {
                    uint8_t r = frame_ptr[p * 4 + 0];
                    uint8_t g = frame_ptr[p * 4 + 1];
                    uint8_t b = frame_ptr[p * 4 + 2];
                    uint8_t a = frame_ptr[p * 4 + 3];

                    if (a == 0) {
                        frame.pixels.push_back(0);
                        frame.pixels.push_back(0);
                        frame.pixels.push_back(0);
                    } else if (a == 255) {
                        frame.pixels.push_back(r);
                        frame.pixels.push_back(g);
                        frame.pixels.push_back(b);
                    } else {
                        frame.pixels.push_back(static_cast<uint8_t>((r * a) / 255));
                        frame.pixels.push_back(static_cast<uint8_t>((g * a) / 255));
                        frame.pixels.push_back(static_cast<uint8_t>((b * a) / 255));
                    }
                }

                anim.frames.push_back(std::move(frame));
            }

            if (delays) STBI_FREE(delays);
            STBI_FREE(data);
            return anim;
        }

        // 2. Décodage d'une image fixe forcée directement en RGB (3 canaux)
        stbi_uc *img = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc *>(bytes.data()),
            static_cast<int>(bytes.size()),
            &w, &h, &comp, 4
        );

        if (img) {
            if (w <= 0 || h <= 0 || w > UINT16_MAX || h > UINT16_MAX) {
                STBI_FREE(img);
                return anim;
            }
            anim.valid = true;
            anim.is_animated = false;

            FrameData frame;
            frame.width = static_cast<uint16_t>(w);
            frame.height = static_cast<uint16_t>(h);
            frame.delay_ms = 0;
            frame.pixels.reserve(static_cast<size_t>(w) * h * 3);
            for (size_t i = 0; i < static_cast<size_t>(w) * h * 4; i += 4) {
                frame.pixels.push_back(img[i]);
                frame.pixels.push_back(img[i + 1]);
                frame.pixels.push_back(img[i + 2]);
            }
            anim.frames.push_back(std::move(frame));
            STBI_FREE(img);
        }

        return anim;
    }

    static RawImageData decode_from_memory(const std::string_view bytes) {
        RawImageData img;
        unsigned char *data = stbi_load_from_memory(
            reinterpret_cast<const uint8_t *>(bytes.data()),
            static_cast<int>(bytes.size()),
            &img.width, &img.height, &img.channels, 3
        );

        if (data) {
            img.pixels.assign(data, data + (img.width * img.height * 3));
            img.valid = true;
            stbi_image_free(data);
        }

        return img;
    }
};
