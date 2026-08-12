// monday/goap/intent_table.h
//
// Intent table schema loader and intent vector table manager.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include <string>

#include "monday/core/errors.h"
#include "monday/core/result.h"

namespace monday {

enum class ActionClass {
    READ_ONLY   = 0,
    WRITE       = 1,
    DESTRUCTIVE = 2,
    SHELL       = 3,
    NETWORK     = 4
};

struct IntentDefinition {
    std::string              id;
    std::vector<std::string> utterances;
    std::string              action;
    ActionClass              action_class;
    std::string              description;
    std::vector<float>       vector; /* Normalized 384-d vector embedding */
};

class IntentTable {
public:
    IntentTable();
    ~IntentTable();

    Result<void> load_intents(const char *jsonl_path);
    const std::vector<IntentDefinition>& get_intents() const { return intents_; }

private:
    std::vector<IntentDefinition> intents_;
};

} // namespace monday
