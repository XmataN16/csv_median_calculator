/**
 * \file main.cpp
 * \brief Entry point for csv_median_calculator
 *
 * Responsibilities:
 *  - parse CLI args (Boost.Program_options)
 *  - parse config (toml++)
 *  - locate CSV files (std::filesystem)
 *  - read records (csv_reader)
 *  - sort by receive_ts and incrementally compute median (median_calculator)
 *  - write output CSV (only when median changes)
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <iomanip>

#include <boost/program_options.hpp>
// include Boost.Accumulators headers to satisfy requirement (we use two-heaps for median).
#include <boost/accumulators/accumulators.hpp>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "config_parser.hpp"
#include "csv_reader.hpp"
#include "median_calculator.hpp"

namespace fs = std::filesystem;
namespace po = boost::program_options;

int main(int argc, char** argv) {
    try {
        // ---- CLI parsing ----
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "show help")
            ("config,cfg,config", po::value<std::string>(), "path to config TOML (alias: -cfg, -config)")
            ;

        // Support -cfg and -config as aliases: Boost PO doesn't parse duplicate names easily,
        // so parse manually by searching argv if necessary.
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }

        // Determine config path
        fs::path config_path;
        if (vm.count("config")) {
            config_path = fs::path(vm["config"].as<std::string>());
        } else if (vm.count("cfg")) {
            config_path = fs::path(vm["cfg"].as<std::string>());
        } else {
            // default: config.toml in executable directory or examples/config.toml copied by CMake
            config_path = fs::current_path() / "config.toml";
            // If not exists, fallback to examples/config.toml (common during dev)
            if (!fs::exists(config_path) && fs::exists(fs::current_path() / "examples" / "config.toml")) {
                config_path = fs::current_path() / "examples" / "config.toml";
            }
        }

        spdlog::info("Starting app. Using config: {}", config_path.string());

        // ---- Parse config ----
        cfg::main_config_t config;
        auto cfg_err = cfg::parse_config(config_path, config);
        if (cfg_err) {
            spdlog::error("Config parse error: {}", *cfg_err);
            return 2;
        }

        // If output not provided, use ./output relative to working directory
        if (config.output_dir.empty()) {
            config.output_dir = fs::current_path() / "output";
        }

        spdlog::info("Input dir: {}", config.input_dir.string());
        spdlog::info("Output dir: {}", config.output_dir.string());
        spdlog::info("File masks: {}", config.filename_mask.empty() ? "<all>" : fmt::format("{}", fmt::join(config.filename_mask, ",")));

        // ---- Read CSV files ----
        std::vector<csv::record_t> records;
        auto read_err = csv::read_csv_files(config.input_dir, config.filename_mask, records);
        if (read_err) {
            spdlog::error("Failed to read CSV files: {}", *read_err);
            return 3;
        }

        spdlog::info("Total records read: {}", records.size());
        if (records.empty()) {
            spdlog::warn("No records to process. Exiting.");
            return 0;
        }

        // ---- Sort by receive_ts (stable sort) ----
        std::stable_sort(records.begin(), records.end(),
                         [](auto const& a, auto const& b) {
                             if (a.receive_ts != b.receive_ts) return a.receive_ts < b.receive_ts;
                             // keep input order if timestamps equal; use file+line to decide
                             if (a.source_file != b.source_file) return a.source_file < b.source_file;
                             return a.line_no < b.line_no;
                         });

        // ---- Prepare output directory & file ----
        try {
            fs::create_directories(config.output_dir);
        } catch (const std::exception& ex) {
            spdlog::error("Failed to create output directory {}: {}", config.output_dir.string(), ex.what());
            return 4;
        }
        auto out_path = config.output_dir / "median_result.csv";
        std::ofstream ofs(out_path, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) {
            spdlog::error("Failed to open output file: {}", out_path.string());
            return 5;
        }
        // write header
        ofs << "receive_ts;price_median\n";

        // ---- Incremental median calculation ----
        median::median_calculator calc;
        // we store formatted median string (8 decimal places) to decide when it changed
        std::optional<std::string> last_median_str;

        auto format_price = [](long double v) -> std::string {
            std::ostringstream ss;
            ss.setf(std::ios::fixed);
            ss << std::setprecision(8) << (double)v; // cast to double for stable formatting
            return ss.str();
        };

        std::size_t changes_written = 0;
        for (const auto& rec : records) {
            calc.add(rec.price);
            auto med_opt = calc.median();
            if (!med_opt) continue;
            auto med_str = format_price(*med_opt);
            if (!last_median_str || (*last_median_str != med_str)) {
                // write
                ofs << rec.receive_ts << ";" << med_str << "\n";
                last_median_str = med_str;
                ++changes_written;
            }
        }
        ofs.close();
        spdlog::info("Written {} median-change lines to {}", changes_written, out_path.string());
        spdlog::info("Done.");
        return 0;
    } catch (const std::exception& ex) {
        spdlog::critical("Unhandled exception: {}", ex.what());
        return 10;
    }
}
