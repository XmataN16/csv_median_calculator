#pragma once
/**
 * \file config_parser.hpp
 * \brief Парсинг конфигурации в формате TOML
 *
 * Функции помогают прочитать секцию [main] и заполнить структуру cfg::main_config_t.
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

    /**
     * \brief Парсит TOML конфиг и заполняет out_config.
     * \param path путь к файлу config.toml
     * \param out_config выходная структура
     * \return std::nullopt при успехе, иначе строка с описанием ошибки
     */
    inline std::optional<std::string> parse_config(const std::filesystem::path& path,
        main_config_t& out_config) {
        try {
            if (!std::filesystem::exists(path)) {
                return std::string("Файл конфигурации не найден: ") + path.string();
            }
            const auto tbl = toml::parse_file(path.string());

            auto main_node = tbl["main"];
            if (!main_node) {
                return std::string("В конфиге отсутствует секция [main]");
            }

            // input (обязательный)
            if (auto in = main_node["input"].value<std::string>(); in) {
                out_config.input_dir = std::filesystem::path(*in);
            }
            else {
                return std::string("Ошибка конфига: 'main.input' обязателен и должен быть строкой");
            }

            // output (опционально)
            if (auto out = main_node["output"].value<std::string>(); out) {
                out_config.output_dir = std::filesystem::path(*out);
            }
            else {
                out_config.output_dir.clear();
            }

            // filename_mask (опционально)
            out_config.filename_mask.clear();
            if (auto masks = main_node["filename_mask"]; masks && masks.is_array()) {
                for (const auto& item : *masks.as_array()) {
                    if (auto s = item.value<std::string>(); s) {
                        out_config.filename_mask.push_back(*s);
                    }
                }
            }
        }
        catch (const toml::parse_error& ex) {
            return std::string("Ошибка парсинга TOML: ") + ex.what();
        }
        catch (const std::exception& ex) {
            return std::string("Исключение при разборе конфига: ") + ex.what();
        }
        return std::nullopt;
    }

}  // namespace cfg
