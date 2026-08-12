// monday/code_intel/tree_sitter_indexer.h
//
// Tree-sitter code intelligence indexer (§2.8, §3.8, Phase 5).
// Extracts function definitions, classes, and imports from source files.

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include "monday/core/result.h"
#include "monday/core/errors.h"

namespace monday {

struct CodeSymbol {
    std::string name;
    std::string kind; // "function", "class", "import"
    uint32_t start_line;
    uint32_t end_line;
};

struct CodeIndexResult {
    std::string file_path;
    std::string language;
    std::vector<CodeSymbol> symbols;
};

class TreeSitterIndexer {
public:
    TreeSitterIndexer();
    ~TreeSitterIndexer();

    // Parse a source code string or file and extract symbols.
    monday::Result<CodeIndexResult> index_content(const std::string &file_path,
                                                  const std::string &content,
                                                  const std::string &language);

    monday::Result<CodeIndexResult> index_file(const std::string &file_path);
};

} // namespace monday
