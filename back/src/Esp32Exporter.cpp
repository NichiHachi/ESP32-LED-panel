#include "Esp32Exporter.h"

uint16_t Esp32Exporter::to_rgb565(const RGB p) {
    return ((p.r & 0xF8) << 8) | ((p.g & 0xFC) << 3) | (p.b >> 3);
}

std::vector<uint8_t> Esp32Exporter::export_binary_rgb888(const Frame &frame, const bool serpentine) {
    std::vector<uint8_t> buffer;
    buffer.reserve(frame.width * frame.height * 3);

    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const int actual_x = (serpentine && (y % 2 != 0)) ? (frame.width - 1 - x) : x;
            const auto [r, g, b] = frame.pixels[y * frame.width + actual_x];

            buffer.push_back(r);
            buffer.push_back(g);
            buffer.push_back(b);
        }
    }
    return buffer;
}
