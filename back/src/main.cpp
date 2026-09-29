#include "crow.h"
#include <nlohmann/json.hpp>

#include "core/AppContext.hpp"
#include "io/ImageDecoder.hpp"
#include "io/ImageEncoder.hpp"

using json = nlohmann::json;

// Fonction utilitaire pour vérifier si le buffer commence par les octets magiques d'un ZIP ("PK\x03\x04")
bool is_zip_file(std::string_view bytes) {
    return bytes.size() >= 4 && bytes[0] == 'P' && bytes[1] == 'K' && bytes[2] == 0x03 && bytes[3] == 0x04;
}

int main() {
    auto app_context = std::make_shared<AppContext>("config.json");
    crow::SimpleApp app;

    CROW_ROUTE(app, "/").methods(crow::HTTPMethod::POST)([&app_context](const crow::request &req) {
        crow::multipart::message msg(req);
        std::string_view image_bytes;
        bool return_matrix = false;
        bool return_zip = false;

        if (req.url_params.get("return_matrix") != nullptr) {
            std::string param = req.url_params.get("return_matrix");
            return_matrix = (param == "true" || param == "1");
        }
        if (req.url_params.get("return_zip") != nullptr) {
            std::string param = req.url_params.get("return_zip");
            return_zip = (param == "true" || param == "1");
        }

        for (const auto &part: msg.parts) {
            if (auto disposition = part.get_header_object("Content-Disposition"); disposition.params.count("name")) {
                std::string name = disposition.params.at("name");

                if (name == "image" || name == "file" || disposition.params.count("filename")) {
                    image_bytes = part.body;
                } else if (name == "return_matrix") {
                    return_matrix = (part.body == "true" || part.body == "1");
                } else if (name == "return_zip") {
                    return_zip = (part.body == "true" || part.body == "1");
                }
            }
        }

        if (image_bytes.empty() && !req.body.empty()) {
            image_bytes = req.body;
        }

        if (image_bytes.empty()) {
            return crow::response(400, "Aucun fichier (image ou ZIP) reçu.");
        }

        // 1. Décodage : Détection automatique ZIP ou Image classique/GIF
        AnimationData anim;
        if (is_zip_file(image_bytes)) {
            anim = ImageDecoder::decode_zip_from_memory(image_bytes);
        } else {
            anim = ImageDecoder::decode_animation_from_memory(image_bytes);
        }

        if (!anim.valid || anim.frames.empty()) {
            return crow::response(400, "Impossible de décoder le fichier ou le contenu du ZIP.");
        }

        // 2. Traitement de toutes les frames par le moteur PixelEngine
        std::vector<Frame> processed_frames;
        processed_frames.reserve(anim.frames.size());

        for (const auto &raw_frame: anim.frames) {
            processed_frames.push_back(
                app_context->process_image(raw_frame.pixels.data(), raw_frame.width, raw_frame.height)
            );
        }

        auto [matrix_cfg, opts] = app_context->get_config();
        const bool is_serpentine = matrix_cfg.enable_serpentine_layout;

        // 3. Cas A : Retour sous forme d'archive ZIP
        if (return_zip) {
            std::vector<uint8_t> zip_bytes = ImageEncoder::encode_frames_to_zip(processed_frames, is_serpentine);
            if (zip_bytes.empty()) {
                return crow::response(500, "Erreur lors de la génération du fichier ZIP.");
            }

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/zip");
            res.set_header("Content-Disposition", "attachment; filename=\"processed_frames.zip\"");
            res.body = std::string(zip_bytes.begin(), zip_bytes.end());
            return res;
        }

        // 4. Cas B : Matrice de pixels (JSON)
        if (return_matrix) {
            json response_json;
            response_json["is_animated"] = anim.is_animated;
            response_json["frame_count"] = processed_frames.size();

            json frames_json = json::array();
            for (size_t i = 0; i < processed_frames.size(); ++i) {
                json frame_obj;
                frame_obj["width"] = processed_frames[i].width;
                frame_obj["height"] = processed_frames[i].height;
                frame_obj["delay_ms"] = anim.frames[i].delay_ms;

                json pixels_array = json::array();
                for (const auto &pixel: processed_frames[i].pixels) {
                    pixels_array.push_back({
                        {"r", pixel.r},
                        {"g", pixel.g},
                        {"b", pixel.b}
                    });
                }
                frame_obj["pixels"] = pixels_array;
                frames_json.push_back(frame_obj);
            }

            response_json["frames"] = frames_json;

            crow::response res(200, response_json.dump());
            res.set_header("Content-Type", "application/json");
            return res;
        }

        // 5. Cas C : Image fixe PNG seule OU liste des frames encodées en Base64
        if (!anim.is_animated) {
            std::vector<uint8_t> png_output = ImageEncoder::encode_to_png(processed_frames[0], is_serpentine);
            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "image/png");
            res.body = std::string(png_output.begin(), png_output.end());
            return res;
        } else {
            json response_json;
            response_json["is_animated"] = true;
            response_json["frame_count"] = processed_frames.size();

            json png_list = json::array();
            for (size_t i = 0; i < processed_frames.size(); ++i) {
                std::vector<uint8_t> png_bytes = ImageEncoder::encode_to_png(processed_frames[i]);

                std::string b64_png = crow::utility::base64encode(
                    reinterpret_cast<const char *>(png_bytes.data()), png_bytes.size()
                );

                png_list.push_back({
                    {"index", i},
                    {"delay_ms", anim.frames[i].delay_ms},
                    {"png_base64", b64_png}
                });
            }
            response_json["frames"] = png_list;

            crow::response res(200, response_json.dump());
            res.set_header("Content-Type", "application/json");
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
