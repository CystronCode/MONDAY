// tests/fuzz/fuzz_intent_parser.cpp
#include "monday/goap/intent_table.h"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <fstream>
#include <filesystem>
#include <unistd.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Generate a unique temporary filename using getpid()
    std::string temp_path = "/tmp/fuzz_intent_table_" + std::to_string(getpid()) + "_" + std::to_string(size) + ".jsonl";
    std::ofstream out(temp_path, std::ios::binary);
    if (!out.is_open()) return 0;
    out.write(reinterpret_cast<const char*>(data), size);
    out.close();

    monday::IntentTable table;
    (void)table.load_intents(temp_path.c_str());

    std::filesystem::remove(temp_path);
    return 0;
}
