// monday/parsers/docx_parser.h
#pragma once

#include "monday/core/result.h"
#include "monday/core/errors.h"
#include <string>

namespace monday {

struct DocxParseResult {
    std::string text_content;
    int paragraph_count{0};
};

class DocxParser {
public:
    DocxParser() = default;
    ~DocxParser() = default;

    static Result<DocxParseResult> parse_docx(const std::string& docx_path);
};

} // namespace monday
