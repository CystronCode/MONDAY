// tests/unit/test_zip_parser.cpp
#include <catch2/catch_test_macros.hpp>
#include "monday/parsers/zip_parser.h"

#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

static void create_test_zip(const std::string& zip_path, const std::string& entry_name, uint32_t comp_size, uint32_t uncomp_size) {
    std::ofstream out(zip_path, std::ios::binary);
    uint32_t sig = 0x04034b50; // Local file header
    uint16_t version = 20;
    uint16_t flags = 0;
    uint16_t method = 8;
    uint16_t mod_time = 0;
    uint16_t mod_date = 0;
    uint32_t crc32 = 0;
    uint16_t name_len = static_cast<uint16_t>(entry_name.length());
    uint16_t extra_len = 0;

    out.write(reinterpret_cast<char*>(&sig), 4);
    out.write(reinterpret_cast<char*>(&version), 2);
    out.write(reinterpret_cast<char*>(&flags), 2);
    out.write(reinterpret_cast<char*>(&method), 2);
    out.write(reinterpret_cast<char*>(&mod_time), 2);
    out.write(reinterpret_cast<char*>(&mod_date), 2);
    out.write(reinterpret_cast<char*>(&crc32), 4);
    out.write(reinterpret_cast<char*>(&comp_size), 4);
    out.write(reinterpret_cast<char*>(&uncomp_size), 4);
    out.write(reinterpret_cast<char*>(&name_len), 2);
    out.write(reinterpret_cast<char*>(&extra_len), 2);
    out.write(entry_name.data(), name_len);

    std::string payload(comp_size, 'A');
    out.write(payload.data(), comp_size);
    out.close();
}

TEST_CASE("Safe Zip Parser validation tests", "[zip_parser]") {
    std::string tmp_dir = (fs::temp_directory_path() / "monday_zip_test").string();
    fs::create_directories(tmp_dir);

    SECTION("Valid zip archive extraction") {
        std::string zip_file = tmp_dir + "/valid.zip";
        std::string extract_target = tmp_dir + "/extracted_valid";
        fs::create_directories(extract_target);

        create_test_zip(zip_file, "document.txt", 100, 150);

        auto res = monday::ZipParser::safe_extract(zip_file, extract_target);
        REQUIRE(res.has_value());
        REQUIRE(res.value().size() == 1);
    }

    SECTION("Zip bomb detection (> 100:1 ratio)") {
        std::string zip_file = tmp_dir + "/bomb.zip";
        std::string extract_target = tmp_dir + "/extracted_bomb";
        fs::create_directories(extract_target);

        // 100 bytes compressed, 500 MB (500,000,000 bytes) uncompressed -> ratio > 100:1
        create_test_zip(zip_file, "bomb.txt", 100, 500 * 1024 * 1024);

        auto res = monday::ZipParser::safe_extract(zip_file, extract_target);
        REQUIRE(!res.has_value());
        REQUIRE(res.error().code == MONDAY_ERR_INVALID_ARG);
    }

    SECTION("Zip slip path traversal detection") {
        std::string zip_file = tmp_dir + "/slip.zip";
        std::string extract_target = tmp_dir + "/extracted_slip";
        fs::create_directories(extract_target);

        // Entry trying to escape target dir using ../
        create_test_zip(zip_file, "../../../tmp/evil.sh", 50, 50);

        auto res = monday::ZipParser::safe_extract(zip_file, extract_target);
        REQUIRE(!res.has_value());
        REQUIRE(res.error().code == MONDAY_ERR_INVALID_ARG);
    }

    fs::remove_all(tmp_dir);
}
