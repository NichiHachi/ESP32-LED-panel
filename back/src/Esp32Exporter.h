#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

class Esp32Exporter {
public:
    static uint16_t to_rgb565(RGB p);

    static std::vector<uint8_t> export_binary_rgb888(const Frame &frame, bool serpentine);
};
