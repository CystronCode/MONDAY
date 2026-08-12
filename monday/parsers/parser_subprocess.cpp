// monday/parsers/parser_subprocess.cpp
#include "monday/parsers/parser_subprocess.h"
#include "monday/parsers/zip_parser.h"
#include "monday/parsers/pdf_parser.h"
#include "monday/parsers/docx_parser.h"
#include "monday/parsers/ocr_tesseract.h"
#include "monday/security/seccomp_filter.h"
#include "monday/core/log.h"
#include "monday/core/platform.h"

#if defined(__linux__)
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#endif

namespace monday {

Result<std::string> ParserSubprocess::run_sandboxed_parser(ParserType type, const std::string& input_path, const std::string& extra_param) {
#if defined(__linux__)
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "Failed to create IPC pipe for sandboxed parser"));
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "Failed to fork parser subprocess"));
    }

    if (pid == 0) { // Child process
        close(pipefd[0]); // Close read end

        // Install seccomp syscall allowlist filter
        monday_install_seccomp_filter();

        std::string result_text;
        int exit_code = 0;

        switch (type) {
            case ParserType::ZIP: {
                auto res = ZipParser::safe_extract(input_path, extra_param);
                if (res.has_value()) {
                    result_text = "EXTRACTED_OK";
                } else {
                    result_text = "ERROR:" + std::to_string(static_cast<int>(res.error().code)) + ":" + res.error().message;
                    exit_code = 1;
                }
                break;
            }
            case ParserType::PDF: {
                auto res = PdfParser::parse_pdf(input_path);
                if (res.has_value()) {
                    result_text = res.value().text_content;
                } else {
                    result_text = "ERROR:" + std::to_string(static_cast<int>(res.error().code));
                    exit_code = 1;
                }
                break;
            }
            case ParserType::DOCX: {
                auto res = DocxParser::parse_docx(input_path);
                if (res.has_value()) {
                    result_text = res.value().text_content;
                } else {
                    result_text = "ERROR:" + std::to_string(static_cast<int>(res.error().code));
                    exit_code = 1;
                }
                break;
            }
            case ParserType::OCR: {
                auto res = OcrTesseract::extract_text_from_image(input_path);
                if (res.has_value()) {
                    result_text = res.value();
                } else {
                    result_text = "ERROR:" + std::to_string(static_cast<int>(res.error().code));
                    exit_code = 1;
                }
                break;
            }
        }

        ssize_t w = write(pipefd[1], result_text.data(), result_text.size());
        (void)w;
        close(pipefd[1]);
        _exit(exit_code);
    }

    // Parent process
    close(pipefd[1]); // Close write end

    char buffer[4096];
    std::string output;
    ssize_t nread = 0;
    while ((nread = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, static_cast<size_t>(nread));
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFSIGNALED(status)) {
        monday_log_write(MONDAY_LOG_SYSTEM, "Sandboxed parser subprocess terminated by signal");
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_PERMISSION_DENIED, "Parser subprocess killed by seccomp sandbox policy"));
    }

    if (output.rfind("ERROR:", 0) == 0) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, output.c_str()));
    }

    return output;

#else
    // Non-Linux or direct execution fallback
    switch (type) {
        case ParserType::ZIP: {
            auto res = ZipParser::safe_extract(input_path, extra_param);
            if (!res.has_value()) return tl::unexpected(res.error());
            return "EXTRACTED_OK";
        }
        case ParserType::PDF: {
            auto res = PdfParser::parse_pdf(input_path);
            if (!res.has_value()) return tl::unexpected(res.error());
            return res.value().text_content;
        }
        case ParserType::DOCX: {
            auto res = DocxParser::parse_docx(input_path);
            if (!res.has_value()) return tl::unexpected(res.error());
            return res.value().text_content;
        }
        case ParserType::OCR: {
            return OcrTesseract::extract_text_from_image(input_path);
        }
    }
    return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "Unknown parser type"));
#endif
}

} // namespace monday
