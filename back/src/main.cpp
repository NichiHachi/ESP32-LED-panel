#include "crow.h"
#include <nlohmann/json.hpp>

#include "AppContext.hpp"
#include "ImageDecoder.hpp"
#include "ImageEncoder.hpp"

using json = nlohmann::json;

int main() {
    auto app_context = std::make_shared<AppContext>("config.json");
    crow::SimpleApp app;

    CROW_ROUTE(app, "/").methods(crow::HTTPMethod::POST)([&app_context](const crow::request &req) {
        crow::multipart::message msg(req);
        std::string_view image_bytes;
        bool return_matrix = false;

        if (req.url_params.get("return_matrix") != nullptr) {
            std::string param = req.url_params.get("return_matrix");
            return_matrix = (param == "true" || param == "1");
        }

        for (const auto &part: msg.parts) {
            if (auto disposition = part.get_header_object("Content-Disposition"); disposition.params.count("name")) {
                std::string name = disposition.params.at("name");

                if (name == "image" || disposition.params.count("filename")) {
                    image_bytes = part.body;
                } else if (name == "return_matrix") {
                    return_matrix = (part.body == "true" || part.body == "1");
                }
            }
        }

        if (image_bytes.empty() && !req.body.empty()) {
            image_bytes = req.body;
        }

        if (image_bytes.empty()) {
            return crow::response(400, "Aucune image reçue.");
        }

        RawImageData raw_img = ImageDecoder::decode_from_memory(image_bytes);
        if (!raw_img.valid) {
            return crow::response(400, "Impossible de décoder l'image transmise.");
        }

        const Frame processed_frame = app_context->process_image(raw_img.pixels.data(), raw_img.width, raw_img.height);

        if (return_matrix) {
            json matrix_res;
            matrix_res["width"] = processed_frame.width;
            matrix_res["height"] = processed_frame.height;

            json pixels_array = json::array();
            for (const auto &pixel: processed_frame.pixels) {
                pixels_array.push_back({
                    {"r", pixel.r},
                    {"g", pixel.g},
                    {"b", pixel.b}
                });
            }
            matrix_res["pixels"] = pixels_array;

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = matrix_res.dump();
            return res;
        } else {
            std::vector<uint8_t> png_output = ImageEncoder::encode_to_png(processed_frame);

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "image/png");
            res.body = std::string(png_output.begin(), png_output.end());
            return res;
        }
    });

    CROW_ROUTE(app, "/api/config").methods(crow::HTTPMethod::GET)([&app_context]() {
        auto [matrix_cfg, opts] = app_context->get_config();

        json response;
        response["matrix"]["width"] = matrix_cfg.width;
        response["matrix"]["height"] = matrix_cfg.height;
        response["matrix"]["max_palette_colors"] = matrix_cfg.max_palette_colors;
        response["matrix"]["enable_serpentine_layout"] = matrix_cfg.enable_serpentine_layout;

        response["options"]["brightness"] = opts.brightness;
        response["options"]["contrast"] = opts.contrast;
        response["options"]["gamma"] = opts.gamma;
        response["options"]["rotation_deg"] = opts.rotation_deg;
        response["options"]["flip_horizontal"] = opts.flip_horizontal;
        response["options"]["flip_vertical"] = opts.flip_vertical;
        response["options"]["fit_mode"] = opts.fit_mode;
        response["options"]["color_mode"] = opts.color_mode;
        response["options"]["background_color"] = json::array({
            opts.background_color.r,
            opts.background_color.g,
            opts.background_color.b
        });

        return crow::response(200, response.dump());
    });

    CROW_ROUTE(app, "/api/config").methods(crow::HTTPMethod::POST)([&app_context](const crow::request &req) {
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            return crow::response(400, "JSON invalide.");
        }

        auto [matrix_cfg, opts] = app_context->get_config();

        json matrix_json = body.contains("matrix") ? body["matrix"] : body;

        if (matrix_json.contains("width"))
            matrix_cfg.width = matrix_json["width"].get<uint16_t>();
        if (matrix_json.contains("height"))
            matrix_cfg.height = matrix_json["height"].get<uint16_t>();
        if (matrix_json.contains("max_palette_colors"))
            matrix_cfg.max_palette_colors = matrix_json["max_palette_colors"].get<size_t>();
        if (matrix_json.contains("enable_serpentine_layout"))
            matrix_cfg.enable_serpentine_layout = matrix_json["enable_serpentine_layout"].get<bool>();

        json opts_json = body.contains("options") ? body["options"] : body;

        if (opts_json.contains("brightness"))
            opts.brightness = opts_json["brightness"].get<float>();
        if (opts_json.contains("contrast"))
            opts.contrast = opts_json["contrast"].get<float>();
        if (opts_json.contains("gamma"))
            opts.gamma = opts_json["gamma"].get<float>();
        if (opts_json.contains("rotation_deg"))
            opts.rotation_deg = opts_json["rotation_deg"].get<int>();
        if (opts_json.contains("flip_horizontal"))
            opts.flip_horizontal = opts_json["flip_horizontal"].get<bool>();
        if (opts_json.contains("flip_vertical"))
            opts.flip_vertical = opts_json["flip_vertical"].get<bool>();

        if (opts_json.contains("fit_mode")) {
            std::string fit_str = opts_json["fit_mode"].get<std::string>();
            if (fit_str == "LETTERBOX") opts.fit_mode = FitMode::LETTERBOX;
            else if (fit_str == "CROP") opts.fit_mode = FitMode::CROP;
            else if (fit_str == "STRETCH") opts.fit_mode = FitMode::STRETCH;
        }

        if (opts_json.contains("color_mode")) {
            std::string color_str = opts_json["color_mode"].get<std::string>();
            if (color_str == "QUANTIZED_KMEANS") opts.color_mode = ColorMode::QUANTIZED_KMEANS;
            else if (color_str == "GRAYSCALE") opts.color_mode = ColorMode::GRAYSCALE;
            else if (color_str == "RGB565") opts.color_mode = ColorMode::RGB565;
        }

        if (opts_json.contains("background_color") && opts_json["background_color"].is_array() && opts_json[
                "background_color"].size() == 3) {
            opts.background_color.r = opts_json["background_color"][0].get<uint8_t>();
            opts.background_color.g = opts_json["background_color"][1].get<uint8_t>();
            opts.background_color.b = opts_json["background_color"][2].get<uint8_t>();
        }

        app_context->update_config(matrix_cfg, opts, true);

        return crow::response(200, "Configuration mise à jour avec succès.");
    });

    app.port(18080).multithreaded().run();
    return 0;
}
