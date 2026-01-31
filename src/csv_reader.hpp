#pragma once
/**
 * \file csv_reader.hpp
 * \brief Утилиты чтения CSV файлов (разделитель ';')
 *
 * Простая, безопасная логика: парсинг заголовка, поиск колонок receive_ts и price,
 * валидация значений и составление vector<record_t>.
 */

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <charconv>

namespace csv {

    struct record_t {
        std::uint64_t receive_ts;
        long double price;
        std::string source_file;
        std::uint64_t line_no;
    };

    /// Разбить строку по разделителю (простая реализация, не обрабатывает кавычки)
    inline std::vector<std::string> split_line(const std::string& line, char sep = ';') {
        std::vector<std::string> out;
        std::string token;
        out.reserve(8);
        std::istringstream ss(line);
        while (std::getline(ss, token, sep)) {
            out.push_back(token);
        }
        return out;
    }

    /// Парсинг long double с проверкой ошибок
    inline bool parse_long_double(const std::string& s, long double& out_val) {
        try {
            size_t idx = 0;
            out_val = std::stold(s, &idx);
            return idx == s.size();
        }
        catch (...) {
            return false;
        }
    }

    /// Парсинг unsigned 64-bit
    inline bool parse_u64(const std::string& s, std::uint64_t& out_val) {
        const char* begin = s.c_str();
        const char* end = begin + s.size();
        unsigned long long tmp = 0;
        auto res = std::from_chars(begin, end, tmp);
        if (res.ec == std::errc() && res.ptr == end) {
            out_val = static_cast<std::uint64_t>(tmp);
            return true;
        }
        return false;
    }

    /**
     * \brief Считает все CSV файлы в директории dir, фильтруя по masks (если пусто — все .csv).
     * \param dir путь к директории
     * \param masks маски по имени файла
     * \param out_records выходной вектор записей
     * \return std::nullopt при успехе или строка с описанием ошибки
     */
    inline std::optional<std::string> read_csv_files(const std::filesystem::path& dir,
        const std::vector<std::string>& masks,
        std::vector<record_t>& out_records) {
        if (!std::filesystem::exists(dir)) {
            return std::string("Входная директория не существует: ") + dir.string();
        }
        if (!std::filesystem::is_directory(dir)) {
            return std::string("Входной путь не является директорией: ") + dir.string();
        }

        out_records.clear();
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            const auto fname = entry.path().filename().string();
            // проверить расширение .csv (регистр игнорируется)
            const auto ext = entry.path().extension().string();
            if (ext.empty()) continue;
            std::string ext_lower = ext;
            std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
            if (ext_lower != ".csv") continue;

            // фильтрация по маскам
            bool pass = masks.empty();
            for (const auto& m : masks) {
                if (fname.find(m) != std::string::npos) { pass = true; break; }
            }
            if (!pass) continue;

            // открыть файл
            std::ifstream ifs(entry.path());
            if (!ifs.is_open()) {
                return std::string("Не удалось открыть CSV файл: ") + entry.path().string();
            }

            std::string header;
            if (!std::getline(ifs, header)) {
                // пустой файл — пропускаем
                continue;
            }
            auto cols = split_line(header, ';');
            int idx_receive = -1, idx_price = -1;
            for (size_t i = 0; i < cols.size(); ++i) {
                auto c = cols[i];
                c.erase(0, c.find_first_not_of(" \t\r\n"));
                c.erase(c.find_last_not_of(" \t\r\n") + 1);
                if (c == "receive_ts") idx_receive = int(i);
                if (c == "price") idx_price = int(i);
            }
            if (idx_receive < 0 || idx_price < 0) {
                return std::string("CSV файл не содержит required columns (receive_ts, price): ") + entry.path().string();
            }

            std::string line;
            std::uint64_t line_no = 1;
            while (std::getline(ifs, line)) {
                ++line_no;
                if (line.empty()) continue;
                auto vals = split_line(line, ';');
                if (vals.size() <= std::max(idx_receive, idx_price)) {
                    return std::string("Неправильная строка (мало колонок) в файле ") + entry.path().string() +
                        " на строке " + std::to_string(line_no);
                }
                std::uint64_t receive_ts = 0;
                long double price = 0.0L;
                if (!parse_u64(vals[idx_receive], receive_ts)) {
                    return std::string("Неверный receive_ts в файле ") + entry.path().string() + " на строке " + std::to_string(line_no);
                }
                if (!parse_long_double(vals[idx_price], price)) {
                    return std::string("Неверный price в файле ") + entry.path().string() + " на строке " + std::to_string(line_no);
                }

                out_records.push_back(record_t{ receive_ts, price, entry.path().string(), line_no });
            }
        }
        return std::nullopt;
    }

}  // namespace csv
