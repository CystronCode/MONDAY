// tests/fuzz/fuzz_zip_parser.cpp
#include "monday/parsers/zip_parser.h"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <fstream>
#include <filesystem>
#include <unistd.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    std::string temp_path = "/tmp/fuzz_zip_" + std::to_string(getpid()) + "_" + std::to_string(size) + ".zip";
    std::ofstream out(temp_path, std::ios::binary);
    if (!out.is_open()) return 0;
    out.write(reinterpret_cast<const char*>(data), size);
    out.close();

    std::string dest_dir = "/tmp/fuzz_zip_out_" + std::to_string(getpid()) + "_" + std::to_string(size);
    std::filesystem::create_directories(dest_dir);

    // Call both inspect_archive and safe_extract to fuzz all parts of ZipParser
    (void)monday::ZipParser::inspect_archive(temp_path);
    (void)monday::ZipParser::safe_extract(temp_path, dest_dir);

    std::filesystem::remove_all(dest_dir);
    std::filesystem::remove(temp_path);
    return 0;
}
