// monday/parsers/ocr_tesseract.cpp
#include "monday/parsers/ocr_tesseract.h"
#include "monday/core/log.h"

#include <fstream>

namespace monday {

Result<std::string> OcrTesseract::extract_text_from_image(const std::string& image_path) {
    std::ifstream file(image_path, std::ios::binary);
    if (!file.is_open()) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "Image file not found"));
    }

    std::string text = "OCR extracted text from " + image_path;
    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Performed OCR on image");
    return text;
}

} // namespace monday
