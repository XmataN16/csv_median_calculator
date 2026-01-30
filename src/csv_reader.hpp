#pragma once
/**
 * \file csv_reader.hpp
 * \brief CSV reading utilities
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

/// Split line by separator (simple, not handling quotes)
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

/// Parse numeric (long double) with error detection
inline bool parse_long_double(const std::string& s, long double& out_val) {
    // use std::from_chars if available for decimal (but from_chars for long double not in many libs)
    try {
        size_t idx = 0;
        out_val = std::stold(s, &idx);
        return idx == s.size();
    } catch (...) {
        return false;
    }
}

/// Parse unsigned 64-bit integer (receive_ts)
inline bool parse_u64(const std::string& s, std::uint64_t& out_val) {
    // use std::from_chars for safety
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

/// Reads CSV files from directory path. Filters by filename masks (if masks empty -> all .csv files).
/// Returns pair: vector<record_t> or error string.
inline std::optional<std::string> read_csv_files(const std::filesystem::path& dir,
                                                 const std::vector<std::string>& masks,
                                                 std::vector<record_t>& out_records) {
    if (!std::filesystem::exists(dir)) {
        return std::string("Input directory does not exist: ") + dir.string();
    }
    if (!std::filesystem::is_directory(dir)) {
        return std::string("Input path is not a directory: ") + dir.string();
    }

    out_records.clear();
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        const auto fname = entry.path().filename().string();
        // check extension .csv (case-insensitive)
        const auto ext = entry.path().extension().string();
        if (ext.size() == 0) continue;
        std::string ext_lower = ext;
        std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
        if (ext_lower != ".csv") continue;

        // mask filtering
        bool pass = masks.empty();
        for (const auto& m : masks) {
            if (fname.find(m) != std::string::npos) { pass = true; break; }
        }
        if (!pass) continue;

        // open and parse CSV
        std::ifstream ifs(entry.path());
        if (!ifs.is_open()) {
            return std::string("Failed to open CSV file: ") + entry.path().string();
        }

        std::string header;
        if (!std::getline(ifs, header)) {
            // empty file -> skip with warning
            continue;
        }
        auto cols = split_line(header, ';');
        int idx_receive = -1, idx_price = -1;
        for (size_t i = 0; i < cols.size(); ++i) {
            auto c = cols[i];
            // trim spaces
            c.erase(0, c.find_first_not_of(" \t\r\n"));
            c.erase(c.find_last_not_of(" \t\r\n") + 1);
            if (c == "receive_ts") idx_receive = int(i);
            if (c == "price") idx_price = int(i);
        }
        if (idx_receive < 0 || idx_price < 0) {
            return std::string("CSV missing required columns (receive_ts, price) in file: ") + entry.path().string();
        }

        std::string line;
        std::uint64_t line_no = 1;
        while (std::getline(ifs, line)) {
            ++line_no;
            if (line.empty()) continue;
            auto vals = split_line(line, ';');
            if (vals.size() <= std::max(idx_receive, idx_price)) {
                // malformed line: skip but report
                return std::string("Malformed CSV (not enough columns) in file ") + entry.path().string() +
                       " at line " + std::to_string(line_no);
            }
            std::uint64_t receive_ts = 0;
            long double price = 0.0L;
            if (!parse_u64(vals[idx_receive], receive_ts)) {
                return std::string("Invalid receive_ts in file ") + entry.path().string() + " at line " + std::to_string(line_no);
            }
            if (!parse_long_double(vals[idx_price], price)) {
                return std::string("Invalid price in file ") + entry.path().string() + " at line " + std::to_string(line_no);
            }

            out_records.push_back(record_t{receive_ts, price, entry.path().string(), line_no});
        }
    } // for each file
    return std::nullopt;
}

}  // namespace csv
