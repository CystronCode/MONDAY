// monday/parsers/zip_parser.h
#pragma once

#include "monday/core/result.h"
#include "monday/core/errors.h"
#include <string>
#include <vector>
#include <cstdint>

namespace monday {

constexpr uint64_t kMaxZipUncompressedSizeBytes = 500 * 1024 * 1024; // 500 MB hard ceiling
constexpr double   kMaxZipCompressionRatio       = 100.0;            // 100:1 max ratio

struct ZipEntryInfo {
    std::string filename;
    uint64_t compressed_size{0};
    uint64_t uncompressed_size{0};
    bool is_directory{false};
};

class ZipParser {
public:
    ZipParser() = default;
    ~ZipParser() = default;

    // Validates an archive for Zip Bomb (size > 500MB or ratio > 100:1) and Zip Slip (path traversal)
    static Result<std::vector<ZipEntryInfo>> inspect_archive(const std::string& zip_path);

    // Safely extracts a zip file to target_dir. Zero files extracted if bomb or slip detected.
    static Result<std::vector<std::string>> safe_extract(const std::string& zip_path, const std::string& target_dir);
};

} // namespace monday
