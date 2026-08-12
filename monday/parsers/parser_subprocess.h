// monday/parsers/parser_subprocess.h
#pragma once

#include "monday/core/result.h"
#include "monday/core/errors.h"
#include <string>

namespace monday {

enum class ParserType {
    ZIP,
    PDF,
    DOCX,
    OCR
};

class ParserSubprocess {
public:
    ParserSubprocess() = default;
    ~ParserSubprocess() = default;

    // Executes parsing task inside a sandboxed subprocess with seccomp filtering enabled
    static Result<std::string> run_sandboxed_parser(ParserType type, const std::string& input_path, const std::string& extra_param = "");
};

} // namespace monday
