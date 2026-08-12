// monday/parsers/ocr_tesseract.h
#pragma once

#include "monday/core/result.h"
#include "monday/core/errors.h"
#include <string>

namespace monday {

class OcrTesseract {
public:
    OcrTesseract() = default;
    ~OcrTesseract() = default;

    static Result<std::string> extract_text_from_image(const std::string& image_path);
};

} // namespace monday
