#pragma once

#include "Types.hpp"

class MatrixExporter {
public:
    static PixelMatrix to_2d_matrix(const Frame& frame);
    static PixelMatrix to_serpentine_matrix(const Frame& frame);
};