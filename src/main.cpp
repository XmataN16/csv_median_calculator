#include <iostream>
#include <string>

#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>
#include <toml++/toml.h>

namespace po = boost::program_options;

int main(int argc, char** argv) {
    // Настройка опций командной строки
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "show help")
        ("config,c", po::value<std::string>()->default_value("examples/config.toml"), "path to config TOML");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing command line: " << e.what() << "\n";
        return 2;
    }

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    const std::string cfg_path = vm["config"].as<std::string>();

    spdlog::info("Starting app. Using config: {}", cfg_path);

    // Пробуем распарсить TOML (smoke-test)
    try {
        const auto tbl = toml::parse_file(cfg_path);
        spdlog::info("Parsed TOML: root contains {} entries", tbl.size());
        // если в файле есть поле example.value, читаем
        if (auto node = tbl["example"]; node) {
            if (auto v = node["value"].value<int>()) {
                spdlog::info("example.value = {}", *v);
            } else {
                spdlog::info("example.value not found or not int");
            }
        }
    } catch (const toml::parse_error& ex) {
        spdlog::error("TOML parse error: {}", ex.what());
        return 3;
    } catch (const std::exception& ex) {
        spdlog::error("Unhandled error: {}", ex.what());
        return 4;
    }

    spdlog::info("Smoke test OK");
    std::cout << "OK\n";
    return 0;
}
