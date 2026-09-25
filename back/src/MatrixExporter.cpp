#include "MatrixExporter.h"

PixelMatrix MatrixExporter::to_2d_matrix(const Frame &frame) {
    PixelMatrix matrix(frame.height, std::vector<RGB>(frame.width));
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            matrix[y][x] = frame.pixels[y * frame.width + x];
        }
    }
    return matrix;
}

PixelMatrix MatrixExporter::to_serpentine_matrix(const Frame &frame) {
    PixelMatrix matrix(frame.height, std::vector<RGB>(frame.width));
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const int actual_x = (y % 2 != 0) ? (frame.width - 1 - x) : x;
            matrix[y][x] = frame.pixels[y * frame.width + actual_x];
        }
    }
    return matrix;
}
