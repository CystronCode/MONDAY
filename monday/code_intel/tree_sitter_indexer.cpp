// monday/code_intel/tree_sitter_indexer.cpp
//
// Tree-sitter code intelligence indexer implementation.

#include "monday/code_intel/tree_sitter_indexer.h"
#include "monday/core/log.h"
#include <tree_sitter/api.h>

#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace monday {

// Forward declarations for Tree-sitter language functions if available or dynamic fallback
extern "C" {
    const TSLanguage *tree_sitter_python(void) __attribute__((weak));
    const TSLanguage *tree_sitter_c(void) __attribute__((weak));
    const TSLanguage *tree_sitter_cpp(void) __attribute__((weak));
}

TreeSitterIndexer::TreeSitterIndexer() {}

TreeSitterIndexer::~TreeSitterIndexer() {}

static void extract_nodes_recursive(TSNode node, const std::string &content, std::vector<CodeSymbol> &symbols) {
    uint32_t child_count = ts_node_child_count(node);
    const char *type = ts_node_type(node);

    if (type) {
        std::string stype(type);
        if (stype == "function_definition" || stype == "function_declarator" || stype == "class_definition" || stype == "class_specifier") {
            CodeSymbol sym;
            sym.kind = (stype.find("class") != std::string::npos) ? "class" : "function";
            TSPoint start = ts_node_start_point(node);
            TSPoint end = ts_node_end_point(node);
            sym.start_line = start.row + 1;
            sym.end_line = end.row + 1;

            // Extract name identifier child if present
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(node, i);
                const char *ctype = ts_node_type(child);
                if (ctype && (std::string(ctype) == "identifier" || std::string(ctype) == "type_identifier" || std::string(ctype) == "name")) {
                    uint32_t sb = ts_node_start_byte(child);
                    uint32_t eb = ts_node_end_byte(child);
                    if (eb <= content.size() && sb < eb) {
                        sym.name = content.substr(sb, eb - sb);
                        break;
                    }
                }
            }
            if (sym.name.empty()) {
                sym.name = "anonymous";
            }
            symbols.push_back(sym);
        } else if (stype == "import_statement" || stype == "import_from_statement" || stype == "preproc_include") {
            CodeSymbol sym;
            sym.kind = "import";
            TSPoint start = ts_node_start_point(node);
            TSPoint end = ts_node_end_point(node);
            sym.start_line = start.row + 1;
            sym.end_line = end.row + 1;
            uint32_t sb = ts_node_start_byte(node);
            uint32_t eb = ts_node_end_byte(node);
            if (eb <= content.size() && sb < eb) {
                sym.name = content.substr(sb, eb - sb);
            }
            symbols.push_back(sym);
        }
    }

    for (uint32_t i = 0; i < child_count; i++) {
        extract_nodes_recursive(ts_node_child(node, i), content, symbols);
    }
}

monday::Result<CodeIndexResult> TreeSitterIndexer::index_content(const std::string &file_path,
                                                                  const std::string &content,
                                                                  const std::string &language) {
    CodeIndexResult res;
    res.file_path = file_path;
    res.language = language;

    TSParser *parser = ts_parser_new();
    if (!parser) {
        return monday::Result<CodeIndexResult>(tl::unexpect,
            MONDAY_ERROR(MONDAY_ERR_NOT_SUPPORTED, "Failed to create Tree-sitter parser"));
    }

    const TSLanguage *lang = nullptr;
    if ((language == "python" || language == "py") && tree_sitter_python) {
        lang = tree_sitter_python();
    } else if ((language == "c") && tree_sitter_c) {
        lang = tree_sitter_c();
    } else if ((language == "cpp" || language == "cxx" || language == "cc") && tree_sitter_cpp) {
        lang = tree_sitter_cpp();
    }

    if (lang) {
        ts_parser_set_language(parser, lang);
    }

    TSTree *tree = ts_parser_parse_string(parser, nullptr, content.c_str(), (uint32_t)content.size());
    if (tree) {
        TSNode root = ts_tree_root_node(tree);
        extract_nodes_recursive(root, content, res.symbols);
        ts_tree_delete(tree);
    } else {
        // Fallback line scanner for symbol extraction if tree is null
        std::istringstream stream(content);
        std::string line;
        uint32_t line_num = 0;
        while (std::getline(stream, line)) {
            line_num++;
            if (line.find("def ") == 0 || line.find("def ") != std::string::npos) {
                CodeSymbol sym;
                sym.kind = "function";
                sym.start_line = line_num;
                sym.end_line = line_num;
                size_t pos = line.find("def ");
                size_t end_p = line.find("(", pos);
                if (end_p != std::string::npos && end_p > pos + 4) {
                    sym.name = line.substr(pos + 4, end_p - (pos + 4));
                } else {
                    sym.name = "function";
                }
                res.symbols.push_back(sym);
            } else if (line.find("class ") == 0 || line.find("class ") != std::string::npos) {
                CodeSymbol sym;
                sym.kind = "class";
                sym.start_line = line_num;
                sym.end_line = line_num;
                size_t pos = line.find("class ");
                size_t end_p = line.find_first_of(":(", pos);
                if (end_p != std::string::npos && end_p > pos + 6) {
                    sym.name = line.substr(pos + 6, end_p - (pos + 6));
                } else {
                    sym.name = "class";
                }
                res.symbols.push_back(sym);
            }
        }
    }

    ts_parser_delete(parser);
    return res;
}

monday::Result<CodeIndexResult> TreeSitterIndexer::index_file(const std::string &file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return monday::Result<CodeIndexResult>(tl::unexpect,
            MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "Failed to open source file"));
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    std::string lang = "c";
    if (file_path.rfind(".py") != std::string::npos) lang = "python";
    else if (file_path.rfind(".cpp") != std::string::npos || file_path.rfind(".hpp") != std::string::npos) lang = "cpp";

    return index_content(file_path, content, lang);
}

} // namespace monday
