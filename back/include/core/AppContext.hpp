#pragma once

#include "core/Types.hpp"
#include "core/PixelEngine.h"
#include "io/ConfigParser.h"
#include <mutex>
#include <stdexcept>
#include <string>

class AppContext {
private:
    mutable std::mutex mutex_;
    LedMatrixConfig matrix_cfg_;
    ProcessingOptions options_;
    std::string config_filepath_;

public:
    explicit AppContext(std::string config_filepath)
        : config_filepath_(std::move(config_filepath)) {
        // unique load at start, save default if not present
        if (!ConfigParser::load_from_file(config_filepath_, matrix_cfg_, options_)) {
            ConfigParser::save_to_file(config_filepath_, matrix_cfg_, options_);
        }
    }

    std::pair<LedMatrixConfig, ProcessingOptions> get_config() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {matrix_cfg_, options_};
    }

    Frame process_image(const unsigned char *raw_data, int src_w, int src_h) {
        LedMatrixConfig matrix_cfg;
        ProcessingOptions options;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            matrix_cfg = matrix_cfg_;
            options = options_;
        }

        PixelEngine engine(matrix_cfg);
        return engine.process_raw_image(raw_data, src_w, src_h, options);
    }

    void update_config(const LedMatrixConfig &new_matrix, const ProcessingOptions &new_opts, bool save_to_disk = true) {
        std::string filepath;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            matrix_cfg_ = new_matrix;
            options_ = new_opts;
            filepath = config_filepath_;
        }

        if (save_to_disk && !ConfigParser::save_to_file(filepath, new_matrix, new_opts)) {
            throw std::runtime_error("Impossible de sauvegarder la configuration.");
        }
    }
};
