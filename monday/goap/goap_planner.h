// monday/goap/goap_planner.h
//
// GOAP Intent Fast-Path Classifier (< 5 ms execution target).

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string>

#include "monday/core/errors.h"
#include "monday/core/result.h"
#include "monday/goap/intent_table.h"
#include "monday/goap/extractors.h"

namespace monday {

struct GOAPResult {
    bool        matched;
    float       confidence;
    std::string intent_id;
    std::string action;
    ActionClass action_class;
    std::string param_name;
    int64_t     param_int_val;
    std::string param_str_val;
};

class GOAPPlanner {
public:
    GOAPPlanner();
    ~GOAPPlanner();

    Result<void> init(const char *intents_jsonl_path);
    GOAPResult classify(const char *query);

private:
    IntentTable table_;
};

} // namespace monday
