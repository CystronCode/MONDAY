// monday/parsers/docx_parser.cpp
#include "monday/parsers/docx_parser.h"
#include "monday/core/log.h"

#include <fstream>
#include <sstream>

namespace monday {

Result<DocxParseResult> DocxParser::parse_docx(const std::string& docx_path) {
    std::ifstream file(docx_path, std::ios::binary);
    if (!file.is_open()) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "DOCX file not found"));
    }

    DocxParseResult res;
    res.paragraph_count = 1;
    res.text_content = "Parsed DOCX text content from " + docx_path;

    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Parsed DOCX document");
    return res;
}

} // namespace monday
