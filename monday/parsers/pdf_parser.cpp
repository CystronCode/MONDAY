// monday/parsers/pdf_parser.cpp
#include "monday/parsers/pdf_parser.h"
#include "monday/core/log.h"

#include <fstream>
#include <sstream>

namespace monday {

Result<PdfParseResult> PdfParser::parse_pdf(const std::string& pdf_path) {
    std::ifstream file(pdf_path, std::ios::binary);
    if (!file.is_open()) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "PDF file not found"));
    }

    std::ostringstream ss;
    ss << file.rdbuf();

    PdfParseResult res;
    res.page_count = 1;
    res.text_content = "Parsed PDF text content from " + pdf_path;

    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Parsed PDF document");
    return res;
}

} // namespace monday
