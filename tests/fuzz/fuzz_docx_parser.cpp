// tests/fuzz/fuzz_docx_parser.cpp
#include "monday/parsers/docx_parser.h"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <fstream>
#include <filesystem>
#include <unistd.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    std::string temp_path = "/tmp/fuzz_docx_" + std::to_string(getpid()) + "_" + std::to_string(size) + ".docx";
    std::ofstream out(temp_path, std::ios::binary);
    if (!out.is_open()) return 0;
    out.write(reinterpret_cast<const char*>(data), size);
    out.close();

    (void)monday::DocxParser::parse_docx(temp_path);

    std::filesystem::remove(temp_path);
    return 0;
}
