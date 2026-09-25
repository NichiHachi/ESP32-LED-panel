#pragma once

#include "Types.hpp"
#include <string>

class ConfigParser {
public:
    static bool load_from_file(
        const std::string &filepath,
        LedMatrixConfig &out_matrix_cfg,
        ProcessingOptions &out_opts
    );

    static bool save_to_file(
        const std::string &filepath,
        const LedMatrixConfig &matrix_cfg,
        const ProcessingOptions &opts
    );

private:
    static FitMode string_to_fit_mode(const std::string &str);

    static ColorMode string_to_color_mode(const std::string &str);
};
