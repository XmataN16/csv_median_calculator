/**
 * \file main.cpp
 * \brief Точка входа csv_median_calculator
 *
 * Задачи:
 *  - парсинг аргументов командной строки (Boost.Program_options)
 *  - поиск и чтение конфигурации (toml++)
 *  - сканирование директории, чтение CSV (csv_reader.hpp)
 *  - сортировка по receive_ts и инкрементальный расчёт медианы (median_calculator.hpp)
 *  - запись результата в CSV (только при изменении медианы)
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <iomanip>

#include <boost/program_options.hpp>
 // Подключаем headers Boost.Accumulators, чтобы удовлетворить требование (включён, но в текущей реализации медиана — две кучи).
#include <boost/accumulators/accumulators.hpp>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "config_parser.hpp"
#include "csv_reader.hpp"
#include "median_calculator.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace po = boost::program_options;

/**
 * \brief Возвращает директорию, где находится исполняемый файл.
 * \return путь к директории exe или пустой путь при ошибке.
 */
static fs::path get_executable_dir() {
    std::string exe_path;
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, buf, MAX_PATH);
    if (len == 0) return {};
    std::wstring w(buf, buf + len);
    exe_path.assign(w.begin(), w.end());
#elif defined(__linux__)
    std::vector<char> buf(4096);
    ssize_t r = readlink("/proc/self/exe", buf.data(), (ssize_t)buf.size());
    if (r <= 0) return {};
    exe_path.assign(buf.data(), buf.data() + r);
#else
    // fallback: текущая рабочая директория
    return fs::current_path();
#endif
    try {
        return fs::path(exe_path).parent_path();
    }
    catch (...) {
        return {};
    }
}

int main(int argc, char** argv) 
{
#if defined(_WIN32)
    // Переключаем кодовую страницу консоли на UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    try {
        // ---- Парсинг аргументов командной строки ----
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "показать подсказку")
            ("config,cfg,config", po::value<std::string>(), "путь к config TOML (алиасы: -cfg, -config)")
            ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }

        // ---- Логика поиска конфигурационного файла ----
        fs::path config_path;
        if (vm.count("config")) {
            config_path = fs::path(vm["config"].as<std::string>());
        }
        else if (vm.count("cfg")) {
            config_path = fs::path(vm["cfg"].as<std::string>());
        }
        else {
            // Попробуем несколько стандартных мест (robust behavior)
            // 1) ./config.toml (cwd)
            fs::path p1 = fs::current_path() / "config.toml";
            if (fs::exists(p1)) {
                config_path = p1;
            }
            else {
                // 2) <exe_dir>/config.toml
                auto exe_dir = get_executable_dir();
                if (!exe_dir.empty()) {
                    fs::path p2 = exe_dir / "config.toml";
                    if (fs::exists(p2)) {
                        config_path = p2;
                    }
                    else {
                        // 3) <exe_dir>/examples/config.toml (CMake копирует examples рядом с exe)
                        fs::path p3 = exe_dir / "examples" / "config.toml";
                        if (fs::exists(p3)) {
                            config_path = p3;
                        }
                        else {
                            // 4) fallback: cwd/examples/config.toml (при запуске из корня проекта)
                            config_path = fs::current_path() / "examples" / "config.toml";
                        }
                    }
                }
                else {
                    // без exe_dir — fallback
                    config_path = fs::current_path() / "examples" / "config.toml";
                }
            }
        }

        spdlog::info("Запуск. Использую конфиг: {}", config_path.string());

        // ---- Разбор конфигурации ----
        cfg::main_config_t config;
        auto cfg_err = cfg::parse_config(config_path, config);
        if (cfg_err) {
            spdlog::error("Ошибка парсинга конфига: {}", *cfg_err);
            return 2;
        }

        // Если output не задан — по умолчанию ./output (cwd)
        if (config.output_dir.empty()) {
            config.output_dir = fs::current_path() / "output";
        }

        spdlog::info("Входная директория: {}", config.input_dir.string());
        spdlog::info("Директория вывода: {}", config.output_dir.string());
        spdlog::info("Фильтр по именам файлов: {}", config.filename_mask.empty() ? "<все>" : fmt::format("{}", fmt::join(config.filename_mask, ",")));

        // ---- Чтение CSV файлов ----
        std::vector<csv::record_t> records;
        auto read_err = csv::read_csv_files(config.input_dir, config.filename_mask, records);
        if (read_err) {
            spdlog::error("Ошибка чтения CSV: {}", *read_err);
            return 3;
        }

        spdlog::info("Прочитано записей: {}", records.size());
        if (records.empty()) {
            spdlog::warn("Нет записей для обработки. Завершение.");
            return 0;
        }

        // ---- Стабильная сортировка по receive_ts (и tie-breaker по файлу/строке) ----
        std::stable_sort(records.begin(), records.end(),
            [](auto const& a, auto const& b) {
                if (a.receive_ts != b.receive_ts) return a.receive_ts < b.receive_ts;
                if (a.source_file != b.source_file) return a.source_file < b.source_file;
                return a.line_no < b.line_no;
            });

        // ---- Подготовка выходного файла ----
        try {
            fs::create_directories(config.output_dir);
        }
        catch (const std::exception& ex) {
            spdlog::error("Не удалось создать директорию вывода {}: {}", config.output_dir.string(), ex.what());
            return 4;
        }

        auto out_path = config.output_dir / "median_result.csv";
        std::ofstream ofs(out_path, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) {
            spdlog::error("Не удалось открыть файл для записи: {}", out_path.string());
            return 5;
        }
        ofs << "receive_ts;price_median\n";

        // ---- Инкрементальный расчёт медианы ----
        median::median_calculator calc;
        std::optional<std::string> last_median_str;

        auto format_price = [](long double v) -> std::string {
            std::ostringstream ss;
            ss.setf(std::ios::fixed);
            ss << std::setprecision(8) << (double)v;
            return ss.str();
            };

        std::size_t changes_written = 0;
        for (const auto& rec : records) {
            calc.add(rec.price);
            auto med_opt = calc.median();
            if (!med_opt) continue;
            auto med_str = format_price(*med_opt);
            if (!last_median_str || (*last_median_str != med_str)) {
                ofs << rec.receive_ts << ";" << med_str << "\n";
                last_median_str = med_str;
                ++changes_written;
            }
        }
        ofs.close();
        spdlog::info("Записано изменений медианы: {} в {}", changes_written, out_path.string());
        spdlog::info("Готово.");
        return 0;
    }
    catch (const std::exception& ex) {
        spdlog::critical("Необработанное исключение: {}", ex.what());
        return 10;
    }
}
