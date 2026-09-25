#pragma once

#include "Types.hpp"

class PixelEngine {
private:
    LedMatrixConfig config;

public:
    explicit PixelEngine(const LedMatrixConfig &cfg);
    Frame process_raw_image(const unsigned char* raw_data, int src_w, int src_h, const ProcessingOptions& opts) const;
};