#pragma once

#include "../Types.hpp"

class ITransform {
public:
    virtual ~ITransform() = default;
    virtual Frame process(const Frame& input, const LedMatrixConfig& config, const ProcessingOptions& opts) = 0;
};
