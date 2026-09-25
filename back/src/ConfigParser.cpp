#include "ConfigParser.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

FitMode ConfigParser::string_to_fit_mode(const std::string &str) {
    if (str == "STRETCH") return FitMode::STRETCH;
    if (str == "CROP") return FitMode::CROP;
    return FitMode::LETTERBOX;
}

ColorMode ConfigParser::string_to_color_mode(const std::string &str) {
    if (str == "GRAYSCALE") return ColorMode::GRAYSCALE;
    if (str == "RGB565") return ColorMode::RGB565;
    return ColorMode::QUANTIZED_KMEANS;
}

bool ConfigParser::load_from_file(
    const std::string &filepath,
    LedMatrixConfig &out_matrix_cfg,
    ProcessingOptions &out_opts
) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[ConfigParser] Impossible d'ouvrir le fichier JSON : " << filepath << std::endl;
        return false;
    }

    try {
        json j;
        file >> j;

        // 1. Parsing de la configuration Matrice
        if (j.contains("matrix")) {
            auto &m = j["matrix"];
            out_matrix_cfg.width = m.value("width", out_matrix_cfg.width);
            out_matrix_cfg.height = m.value("height", out_matrix_cfg.height);
            out_matrix_cfg.max_palette_colors = m.value("max_palette_colors", out_matrix_cfg.max_palette_colors);
            out_matrix_cfg.enable_serpentine_layout = m.value("enable_serpentine_layout",
                                                              out_matrix_cfg.enable_serpentine_layout);
        }

        // 2. Parsing des options de traitement
        if (j.contains("options")) {
            auto &o = j["options"];

            if (o.contains("fit_mode")) {
                out_opts.fit_mode = string_to_fit_mode(o["fit_mode"].get<std::string>());
            }
            if (o.contains("color_mode")) {
                out_opts.color_mode = string_to_color_mode(o["color_mode"].get<std::string>());
            }
            if (o.contains("background_color") && o["background_color"].is_array() && o["background_color"].size() >=
                3) {
                out_opts.background_color = {
                    .r = o["background_color"][0].get<uint8_t>(),
                    .g = o["background_color"][1].get<uint8_t>(),
                    .b = o["background_color"][2].get<uint8_t>()
                };
            }

            out_opts.brightness = o.value("brightness", out_opts.brightness);
            out_opts.contrast = o.value("contrast", out_opts.contrast);
            out_opts.gamma = o.value("gamma", out_opts.gamma);
            out_opts.rotation_deg = o.value("rotation_deg", out_opts.rotation_deg);
            out_opts.flip_horizontal = o.value("flip_horizontal", out_opts.flip_horizontal);
            out_opts.flip_vertical = o.value("flip_vertical", out_opts.flip_vertical);
        }

        return true;
    } catch (const json::exception &e) {
        std::cerr << "[ConfigParser] Erreur lors de l'analyse du JSON : " << e.what() << std::endl;
        return false;
    }
}

bool ConfigParser::save_to_file(
    const std::string &filepath,
    const LedMatrixConfig &matrix_cfg,
    const ProcessingOptions &opts
) {
    nlohmann::json j;

    j["matrix"]["width"] = matrix_cfg.width;
    j["matrix"]["height"] = matrix_cfg.height;
    j["matrix"]["max_palette_colors"] = matrix_cfg.max_palette_colors;
    j["matrix"]["enable_serpentine_layout"] = matrix_cfg.enable_serpentine_layout;

    j["options"]["brightness"] = opts.brightness;
    j["options"]["contrast"] = opts.contrast;
    j["options"]["gamma"] = opts.gamma;
    j["options"]["rotation_deg"] = opts.rotation_deg;
    j["options"]["flip_horizontal"] = opts.flip_horizontal;
    j["options"]["flip_vertical"] = opts.flip_vertical;
    j["options"]["background_color"] = {opts.background_color.r, opts.background_color.g, opts.background_color.b};

    // Conversion enum vers string
    if (opts.fit_mode == FitMode::STRETCH) j["options"]["fit_mode"] = "STRETCH";
    else if (opts.fit_mode == FitMode::CROP) j["options"]["fit_mode"] = "CROP";
    else j["options"]["fit_mode"] = "LETTERBOX";

    if (opts.color_mode == ColorMode::GRAYSCALE) j["options"]["color_mode"] = "GRAYSCALE";
    else if (opts.color_mode == ColorMode::RGB565) j["options"]["color_mode"] = "RGB565";
    else j["options"]["color_mode"] = "QUANTIZED_KMEANS";

    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << j.dump(4);
    return true;
}
