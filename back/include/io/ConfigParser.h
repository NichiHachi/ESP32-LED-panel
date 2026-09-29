#pragma once

#include "core/Types.hpp"
#include <optional>
#include <string>
#include <string_view>

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

    static std::optional<FitMode> parse_fit_mode(std::string_view value);

    static std::optional<ColorMode> parse_color_mode(std::string_view value);
};
