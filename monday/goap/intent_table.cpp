// monday/goap/intent_table.cpp
//
// Intent table JSONL parser and vector embedding generator.

#include "monday/goap/intent_table.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"

#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace monday {

static const size_t VECTOR_DIM = 384;

/* Compute 384-dimensional feature vector for text query */
static std::vector<float> embed_text(const std::string &text) {
    std::vector<float> vec(VECTOR_DIM, 0.0f);
    if (text.empty()) return vec;

    std::string lowered = text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);

    /* Feature hash embedding over words & character n-grams */
    std::istringstream iss(lowered);
    std::string word;
    while (iss >> word) {
        uint32_t hash = 5381;
        for (char c : word) {
            hash = ((hash << 5) + hash) + static_cast<uint8_t>(c);
        }
        vec[hash % VECTOR_DIM] += 1.0f;
    }

    /* Normalize to unit length */
    double norm_sq = 0.0;
    for (float f : vec) norm_sq += f * f;
    if (norm_sq > 1e-9) {
        float norm = static_cast<float>(std::sqrt(norm_sq));
        for (float &f : vec) f /= norm;
    }

    return vec;
}

IntentTable::IntentTable() {}
IntentTable::~IntentTable() {}

Result<void> IntentTable::load_intents(const char *jsonl_path) {
    std::string path_str = jsonl_path ? jsonl_path : "data/intents.jsonl";
    std::ifstream file(path_str);
    if (!file.is_open()) {
        /* Fallback search paths for test binaries */
        std::vector<std::string> search_paths = {
            "../data/intents.jsonl",
            "../../data/intents.jsonl",
            "../../../data/intents.jsonl"
        };
        for (const auto &sp : search_paths) {
            file.open(sp);
            if (file.is_open()) break;
        }
    }

    if (!file.is_open()) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "intents.jsonl file not found"));
    }

    intents_.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        IntentDefinition intent;
        auto extract_json_str = [&](const std::string &key) -> std::string {
            size_t pos = line.find("\"" + key + "\":");
            if (pos == std::string::npos) return "";
            size_t start = line.find("\"", pos + key.size() + 3);
            if (start == std::string::npos) return "";
            size_t end = line.find("\"", start + 1);
            if (end == std::string::npos) return "";
            return line.substr(start + 1, end - start - 1);
        };

        intent.id = extract_json_str("id");
        intent.action = extract_json_str("action");
        intent.description = extract_json_str("description");
        std::string cls_str = extract_json_str("action_class");

        if (cls_str == "READ_ONLY") intent.action_class = ActionClass::READ_ONLY;
        else if (cls_str == "WRITE") intent.action_class = ActionClass::WRITE;
        else if (cls_str == "DESTRUCTIVE") intent.action_class = ActionClass::DESTRUCTIVE;
        else if (cls_str == "SHELL") intent.action_class = ActionClass::SHELL;
        else if (cls_str == "NETWORK") intent.action_class = ActionClass::NETWORK;
        else intent.action_class = ActionClass::WRITE;

        /* Extract utterances array */
        size_t u_pos = line.find("\"utterances\":");
        if (u_pos != std::string::npos) {
            size_t arr_start = line.find("[", u_pos);
            size_t arr_end = line.find("]", u_pos);
            if (arr_start != std::string::npos && arr_end != std::string::npos) {
                std::string sub = line.substr(arr_start + 1, arr_end - arr_start - 1);
                std::istringstream ss(sub);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    size_t s = item.find("\"");
                    size_t e = item.rfind("\"");
                    if (s != std::string::npos && e != std::string::npos && e > s) {
                        intent.utterances.push_back(item.substr(s + 1, e - s - 1));
                    }
                }
            }
        }

        std::string combined_text = intent.id + " ";
        for (const auto &u : intent.utterances) combined_text += u + " ";
        intent.vector = embed_text(combined_text);

        if (!intent.id.empty()) {
            intents_.push_back(intent);
        }
    }

    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "IntentTable: loaded intents successfully");
    return {};
}

} // namespace monday
