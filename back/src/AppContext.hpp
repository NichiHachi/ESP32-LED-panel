#pragma once

#include "Types.hpp"
#include "PixelEngine.h"
#include "ConfigParser.h"
#include <mutex>
#include <memory>
#include <string>

class AppContext {
private:
    mutable std::mutex mutex_;
    LedMatrixConfig matrix_cfg_;
    ProcessingOptions options_;
    std::unique_ptr<PixelEngine> engine_;
    std::string config_filepath_;

public:
    explicit AppContext(std::string config_filepath)
        : config_filepath_(std::move(config_filepath)) {
        // unique load at start, save default if not present
        if (!ConfigParser::load_from_file(config_filepath_, matrix_cfg_, options_)) {
            ConfigParser::save_to_file(config_filepath_, matrix_cfg_, options_);
        }
        engine_ = std::make_unique<PixelEngine>(matrix_cfg_);
    }

    std::pair<LedMatrixConfig, ProcessingOptions> get_config() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {matrix_cfg_, options_};
    }

    Frame process_image(const unsigned char *raw_data, int src_w, int src_h) {
        std::lock_guard<std::mutex> lock(mutex_);
        return engine_->process_raw_image(raw_data, src_w, src_h, options_);
    }

    void update_config(const LedMatrixConfig &new_matrix, const ProcessingOptions &new_opts, bool save_to_disk = true) {
        std::lock_guard<std::mutex> lock(mutex_);

        matrix_cfg_ = new_matrix;
        options_ = new_opts;

        engine_ = std::make_unique<PixelEngine>(matrix_cfg_);

        if (save_to_disk) {
            ConfigParser::save_to_file(config_filepath_, matrix_cfg_, options_);
        }
    }
};
