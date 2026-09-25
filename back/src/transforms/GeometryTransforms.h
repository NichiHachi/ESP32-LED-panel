#pragma once
#include "../Types.hpp"

class GeometryProcessor {
public:
    static Frame apply(const Frame& src, const ProcessingOptions& opts);
};