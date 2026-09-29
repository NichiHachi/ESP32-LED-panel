#include "crow.h"
#include <nlohmann/json.hpp>

#include "core/AppContext.hpp"
#include "io/ImageDecoder.hpp"
#include "io/ImageEncoder.hpp"
#include "io/ConfigParser.h"
#include <cmath>

using json = nlohmann::json;

namespace {
constexpr size_t max_upload_size = 32 * 1024 * 1024; // limited to 32 MB
constexpr int max_matrix_dimension = 1024;
constexpr int max_palette_size = 256;

bool is_zip_file(const std::string_view bytes) {
    return bytes.size() >= 4 && bytes[0] == 'P' && bytes[1] == 'K' && bytes[2] == 0x03 && bytes[3] == 0x04;
}

void apply_config_update(
    const json &body,
    LedMatrixConfig &matrix_cfg,
    ProcessingOptions &opts
) {
    const json matrix_json = body.contains("matrix") ? body.at("matrix") : body;
    const json opts_json = body.contains("options") ? body.at("options") : body;

    if (!matrix_json.is_object() || !opts_json.is_object()) {
        throw json::type_error::create(302, "matrix/options must be objects", &body);
    }

    if (matrix_json.contains("width")) matrix_cfg.width = matrix_json.at("width").get<int>();
    if (matrix_json.contains("height")) matrix_cfg.height = matrix_json.at("height").get<int>();
    if (matrix_json.contains("max_palette_colors")) {
        matrix_cfg.max_palette_colors = matrix_json.at("max_palette_colors").get<int>();
    }
    if (matrix_json.contains("enable_serpentine_layout")) {
        matrix_cfg.enable_serpentine_layout = matrix_json.at("enable_serpentine_layout").get<bool>();
    }

    if (opts_json.contains("brightness")) opts.brightness = opts_json.at("brightness").get<float>();
    if (opts_json.contains("contrast")) opts.contrast = opts_json.at("contrast").get<float>();
    if (opts_json.contains("gamma")) opts.gamma = opts_json.at("gamma").get<float>();
    if (opts_json.contains("rotation_deg")) opts.rotation_deg = opts_json.at("rotation_deg").get<int>();
    if (opts_json.contains("flip_horizontal")) opts.flip_horizontal = opts_json.at("flip_horizontal").get<bool>();
    if (opts_json.contains("flip_vertical")) opts.flip_vertical = opts_json.at("flip_vertical").get<bool>();

    if (opts_json.contains("fit_mode")) {
        const auto mode = ConfigParser::parse_fit_mode(opts_json.at("fit_mode").get<std::string>());
        if (!mode) throw json::out_of_range::create(401, "invalid fit_mode", &opts_json.at("fit_mode"));
        opts.fit_mode = *mode;
    }
    if (opts_json.contains("color_mode")) {
        const auto mode = ConfigParser::parse_color_mode(opts_json.at("color_mode").get<std::string>());
        if (!mode) throw json::out_of_range::create(401, "invalid color_mode", &opts_json.at("color_mode"));
        opts.color_mode = *mode;
    }
    if (opts_json.contains("background_color")) {
        const auto &color = opts_json.at("background_color");
        if (!color.is_array() || color.size() != 3) {
            throw json::type_error::create(302, "background_color must contain 3 values", &color);
        }
        opts.background_color = {
            color.at(0).get<uint8_t>(),
            color.at(1).get<uint8_t>(),
            color.at(2).get<uint8_t>()
        };
    }

    if (matrix_cfg.width <= 0 || matrix_cfg.width > max_matrix_dimension ||
        matrix_cfg.height <= 0 || matrix_cfg.height > max_matrix_dimension) {
        throw json::out_of_range::create(401, "matrix dimensions are out of range", &body);
    }
    if (matrix_cfg.max_palette_colors <= 0 || matrix_cfg.max_palette_colors > max_palette_size) {
        throw json::out_of_range::create(401, "max_palette_colors is out of range", &body);
    }
    if (!std::isfinite(opts.brightness) || opts.brightness < 0.0f ||
        !std::isfinite(opts.contrast) || opts.contrast < 0.0f ||
        !std::isfinite(opts.gamma) || opts.gamma <= 0.0f) {
        throw json::out_of_range::create(401, "color correction values are invalid", &body);
    }
    if (opts.rotation_deg % 90 != 0) {
        throw json::out_of_range::create(401, "rotation_deg must be a multiple of 90", &body);
    }
}
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
        if (image_bytes.size() > max_upload_size) {
            return crow::response(413, "Fichier trop volumineux.");
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
        response["options"]["fit_mode"] = opts.fit_mode == FitMode::CROP
                                               ? "CROP"
                                               : opts.fit_mode == FitMode::STRETCH ? "STRETCH" : "LETTERBOX";
        response["options"]["color_mode"] = opts.color_mode == ColorMode::GRAYSCALE
                                                ? "GRAYSCALE"
                                                : opts.color_mode == ColorMode::RGB565 ? "RGB565" : "QUANTIZED_KMEANS";
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

        try {
            apply_config_update(body, matrix_cfg, opts);
            app_context->update_config(matrix_cfg, opts, true);
        } catch (const json::exception &e) {
                return crow::response(400, std::string("Configuration invalide : ") + e.what());
        } catch (const std::runtime_error &e) {
                return crow::response(500, e.what());
        }

        return crow::response(200, "Configuration mise à jour avec succès.");
    });

    app.port(18080).multithreaded().run();
    return 0;
}
