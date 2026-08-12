// monday/parsers/pdf_parser.h
#pragma once

#include "monday/core/result.h"
#include "monday/core/errors.h"
#include <string>

namespace monday {

struct PdfParseResult {
    std::string text_content;
    int page_count{0};
};

class PdfParser {
public:
    PdfParser() = default;
    ~PdfParser() = default;

    static Result<PdfParseResult> parse_pdf(const std::string& pdf_path);
};

} // namespace monday
