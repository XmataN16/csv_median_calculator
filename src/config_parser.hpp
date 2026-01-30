#pragma once
/**
 * \file config_parser.hpp
 * \brief TOML config parsing helpers
 */

#include <string>
#include <vector>
#include <filesystem>
#include <optional>

#include <toml++/toml.h>

namespace cfg {

struct main_config_t {
    std::filesystem::path input_dir;
    std::filesystem::path output_dir;
    std::vector<std::string> filename_mask;
};

/// Parse TOML config located at path. Returns error string on failure.
inline std::optional<std::string> parse_config(const std::filesystem::path& path,
                                               main_config_t& out_config) {
    try {
        if (!std::filesystem::exists(path)) {
            return std::string("Config file not found: ") + path.string();
        }
        const auto tbl = toml::parse_file(path.string());

        auto main_node = tbl["main"];
        if (!main_node) {
            return std::string("Missing [main] section in config");
        }

        // input (required)
        if (auto in = main_node["input"].value<std::string>(); in) {
            out_config.input_dir = std::filesystem::path(*in);
        } else {
            return std::string("Config error: 'main.input' is required and must be a string");
        }

        // output (optional)
        if (auto out = main_node["output"].value<std::string>(); out) {
            out_config.output_dir = std::filesystem::path(*out);
        } else {
            // default -> 'output' next to executable (handled by caller)
            out_config.output_dir.clear();
        }

        // filename_mask (optional array of strings)
        out_config.filename_mask.clear();
        if (auto masks = main_node["filename_mask"]; masks && masks.is_array()) {
            for (const auto& item : *masks.as_array()) {
                if (auto s = item.value<std::string>(); s) {
                    out_config.filename_mask.push_back(*s);
                }
            }
        }
    } catch (const toml::parse_error& ex) {
        return std::string("TOML parse error: ") + ex.what();
    } catch (const std::exception& ex) {
        return std::string("Config parse exception: ") + ex.what();
    }
    return std::nullopt;
}

}  // namespace cfg
