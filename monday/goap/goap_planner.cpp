// monday/goap/goap_planner.cpp
//
// GOAP Intent Fast-Path Classifier implementation with cosine similarity & parameter extraction.

#include "monday/goap/goap_planner.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"

#include <cmath>
#include <algorithm>
#include <sstream>

namespace monday {

static const size_t VECTOR_DIM = 384;

static std::vector<float> embed_query(const std::string &query) {
    std::vector<float> vec(VECTOR_DIM, 0.0f);
    if (query.empty()) return vec;

    std::string lowered = query;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);

    std::istringstream iss(lowered);
    std::string word;
    while (iss >> word) {
        uint32_t hash = 5381;
        for (char c : word) {
            hash = ((hash << 5) + hash) + static_cast<uint8_t>(c);
        }
        vec[hash % VECTOR_DIM] += 1.0f;
    }

    double norm_sq = 0.0;
    for (float f : vec) norm_sq += f * f;
    if (norm_sq > 1e-9) {
        float norm = static_cast<float>(std::sqrt(norm_sq));
        for (float &f : vec) f /= norm;
    }

    return vec;
}

static float compute_cosine_similarity(const std::vector<float> &a, const std::vector<float> &b) {
    if (a.size() != b.size() || a.empty()) return 0.0f;
    float dot = 0.0f;
    for (size_t i = 0; i < a.size(); i++) {
        dot += a[i] * b[i];
    }
    /* Normalization to [0, 1] range per Architecture §2.5 */
    return std::max(0.0f, std::min(1.0f, (dot + 1.0f) / 2.0f));
}

GOAPPlanner::GOAPPlanner() {}
GOAPPlanner::~GOAPPlanner() {}

Result<void> GOAPPlanner::init(const char *intents_jsonl_path) {
    return table_.load_intents(intents_jsonl_path);
}

GOAPResult GOAPPlanner::classify(const char *query) {
    GOAPResult res{};
    res.matched = false;
    res.confidence = 0.0f;

    if (!query) return res;

    std::string query_str(query);
    std::vector<float> q_vec = embed_query(query_str);

    const auto &intents = table_.get_intents();
    float best_sim = 0.0f;
    const IntentDefinition *best_intent = nullptr;

    for (const auto &intent : intents) {
        /* Check cosine similarity against main intent vector */
        float sim = compute_cosine_similarity(q_vec, intent.vector);

        /* Check individual utterance matches */
        for (const auto &u : intent.utterances) {
            std::vector<float> u_vec = embed_query(u);
            float u_sim = compute_cosine_similarity(q_vec, u_vec);
            if (u_sim > sim) sim = u_sim;

            /* Exact string substring match boost */
            std::string q_low = query_str;
            std::string u_low = u;
            std::transform(q_low.begin(), q_low.end(), q_low.begin(), ::tolower);
            std::transform(u_low.begin(), u_low.end(), u_low.begin(), ::tolower);
            if (q_low.find(u_low) != std::string::npos || u_low.find(q_low) != std::string::npos) {
                sim = std::max(sim, 0.92f);
            }
        }

        if (sim > best_sim) {
            best_sim = sim;
            best_intent = &intent;
        }
    }

    if (best_intent) {
        float required_threshold = (best_intent->action_class == ActionClass::DESTRUCTIVE) ? 0.95f : 0.85f;
        if (best_sim >= required_threshold) {
            res.matched = true;
            res.confidence = best_sim;
            res.intent_id = best_intent->id;
            res.action = best_intent->action;
            res.action_class = best_intent->action_class;

            /* Parameter extraction */
            if (best_intent->id == "set_timer" || best_intent->id == "pause_timer") {
                res.param_name = "duration_seconds";
                monday_extractor_value_t ext_val;
                if (parse_duration_from_query(query, &ext_val).code == MONDAY_OK) {
                    res.param_int_val = ext_val.val.i64;
                }
            } else if (best_intent->id == "open_file" || best_intent->id == "delete_file") {
                res.param_name = "filename";
                monday_extractor_value_t ext_val;
                if (parse_filename_from_query(query, &ext_val).code == MONDAY_OK) {
                    res.param_str_val = ext_val.val.str;
                }
            }
        }
    }

    return res;
}

} // namespace monday
