# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)
project(Monday VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Hardened warning flags
add_compile_options(-Wall -Wextra -Werror -pedantic)

# Module includes
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(Sanitizers)
include(ModuleBoundaries)

# Global include roots
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_BINARY_DIR}/src/generated)
include_directories(${CMAKE_SOURCE_DIR}/src/generated)

# ── third-party: tl::expected ────────────────────────────────────────────────
add_library(tl_expected INTERFACE)
target_include_directories(tl_expected INTERFACE
    ${CMAKE_SOURCE_DIR}/third_party/expected/include)

# ── third-party: Unity (C unit tests) ────────────────────────────────────────
add_library(unity STATIC
    third_party/unity/src/unity.c)
target_include_directories(unity PUBLIC
    third_party/unity/src)

# ── third-party: Catch2 (C++ unit tests) ─────────────────────────────────────
add_subdirectory(third_party/catch2)

# ── Blackboard code generation ────────────────────────────────────────────────
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)

add_custom_command(
    OUTPUT
        ${CMAKE_SOURCE_DIR}/src/generated/blackboard_accessors.h
        ${CMAKE_SOURCE_DIR}/src/generated/blackboard_accessors.c
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tools/generate_blackboard.py
        ${CMAKE_SOURCE_DIR}/blackboard_schema.toml
        ${CMAKE_SOURCE_DIR}/src/generated
    DEPENDS
        ${CMAKE_SOURCE_DIR}/blackboard_schema.toml
        ${CMAKE_SOURCE_DIR}/tools/generate_blackboard.py
    COMMENT "Generating Blackboard accessors from schema"
    VERBATIM
)

add_custom_target(generate_blackboard ALL
    DEPENDS
        ${CMAKE_SOURCE_DIR}/src/generated/blackboard_accessors.h
        ${CMAKE_SOURCE_DIR}/src/generated/blackboard_accessors.c
)

# ── Proto compilation ─────────────────────────────────────────────────────────
find_package(Protobuf REQUIRED)

add_custom_command(
    OUTPUT
        ${CMAKE_SOURCE_DIR}/src/generated/monday_ipc_v1.pb.cc
        ${CMAKE_SOURCE_DIR}/src/generated/monday_ipc_v1.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        --cpp_out=${CMAKE_SOURCE_DIR}/src/generated
        --proto_path=${CMAKE_SOURCE_DIR}/proto
        ${CMAKE_SOURCE_DIR}/proto/monday_ipc_v1.proto
    DEPENDS ${CMAKE_SOURCE_DIR}/proto/monday_ipc_v1.proto
    COMMENT "Compiling Protobuf schema"
    VERBATIM
)

add_custom_target(generate_proto ALL
    DEPENDS
        ${CMAKE_SOURCE_DIR}/src/generated/monday_ipc_v1.pb.cc
        ${CMAKE_SOURCE_DIR}/src/generated/monday_ipc_v1.pb.h
)

# ── monday_core_lib (Phase 0 + Phase 1 core) ─────────────────────────────────
add_library(monday_core_lib STATIC
    monday/core/log.c
    monday/core/epoch.c
    monday/core/allocator.c
    src/generated/blackboard_accessors.c
)
add_dependencies(monday_core_lib generate_blackboard)
target_include_directories(monday_core_lib PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/generated)
target_link_libraries(monday_core_lib PUBLIC tl_expected)

# ── monday_memory_lib (Phase 1 — DOOM Zone + TLSF) ───────────────────────────
add_library(monday_memory_lib STATIC
    monday/memory/zone.c
    monday/memory/tlsf_pool.c
)
target_include_directories(monday_memory_lib PUBLIC ${CMAKE_SOURCE_DIR})
# monday_memory_lib only needs the headers from monday_core_lib, not the link
target_include_directories(monday_memory_lib PUBLIC ${CMAKE_SOURCE_DIR}/src/generated)
# Propagate monday_memory_lib to all consumers of monday_core_lib via INTERFACE
target_link_libraries(monday_core_lib INTERFACE monday_memory_lib)

# Find libsodium and sqlcipher using PkgConfig
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium)
pkg_check_modules(SQLCIPHER REQUIRED sqlcipher)

# ── monday_security_lib (Phase 1 + Phase 4 — Ed25519, model_verify, consent, seccomp)
add_library(monday_security_lib STATIC
    monday/security/ed25519_verify.c
    monday/security/model_verify.cpp
    monday/security/kdf.cpp
    monday/security/keystore.cpp
    monday/security/audit_log.cpp
    monday/security/consent.cpp
    monday/security/seccomp_filter.c
)
target_include_directories(monday_security_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(monday_security_lib PUBLIC monday_core_lib monday_memory_lib ${SODIUM_LIBRARIES} ${SQLCIPHER_LIBRARIES})
target_compile_definitions(monday_security_lib PUBLIC MONDAY_HAVE_LIBSODIUM)

# ── CPUID kernel compile options ──────────────────────────────────────────────
# The three kernel variants are compiled separately with different SIMD flags.
# The dispatch table (cpuid_dispatch.c) and scalar kernel use no SIMD flags.
# The baseline for production builds is -march=x86-64-v2 (SSE4.2+POPCNT).

add_library(monday_kernel_avx2 OBJECT
    monday/inference/kernels/matmul_avx2.c
)
target_compile_options(monday_kernel_avx2 PRIVATE -mavx2)
target_include_directories(monday_kernel_avx2 PRIVATE ${CMAKE_SOURCE_DIR})

add_library(monday_kernel_sse4 OBJECT
    monday/inference/kernels/matmul_sse4.c
)
target_compile_options(monday_kernel_sse4 PRIVATE -msse4.2 -msse4.1)
target_include_directories(monday_kernel_sse4 PRIVATE ${CMAKE_SOURCE_DIR})

add_library(monday_kernel_scalar OBJECT
    monday/inference/kernels/matmul_scalar.c
)
target_include_directories(monday_kernel_scalar PRIVATE ${CMAKE_SOURCE_DIR})

# ── third-party: llama.cpp ───────────────────────────────────────────────────
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/llama.cpp EXCLUDE_FROM_ALL)

# ── monday_inference_lib (Phase 1 — IInferenceEngine + backends + dispatch) ──
add_library(monday_inference_lib STATIC
    monday/inference/cpuid_dispatch.c
    monday/inference/backends/bitnet_engine.cpp
    monday/inference/backends/llama_engine.cpp
    monday/inference/engine_selector.cpp
    $<TARGET_OBJECTS:monday_kernel_avx2>
    $<TARGET_OBJECTS:monday_kernel_sse4>
    $<TARGET_OBJECTS:monday_kernel_scalar>
)
target_include_directories(monday_inference_lib PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/include
    ${CMAKE_SOURCE_DIR}/third_party/llama.cpp/ggml/include
)
target_link_libraries(monday_inference_lib PUBLIC
    monday_core_lib
    monday_memory_lib
    monday_security_lib
    llama
)

# ── monday_voice_lib (Phase 2 — Audio IO + Bandpass + VAD + STT/TTS) ─────────
add_library(monday_voice_lib STATIC
    monday/core/spsc_ring.c
    monday/voice/bandpass_filter.c
    monday/voice/audio_io.cpp
    monday/voice/hotplug_monitor.cpp
    monday/inference/onnx_session.cpp
    monday/voice/vad_webrtc.cpp
    monday/voice/vad_silero.cpp
    monday/voice/stt_whisper.cpp
    monday/voice/tts_piper.cpp
)
target_include_directories(monday_voice_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(monday_voice_lib PUBLIC monday_core_lib pthread)

# ── monday_goap_lib (Phase 2 — Intent Table + GOAP Planner + Extractors) ──────
add_library(monday_goap_lib STATIC
    monday/goap/extractors.c
    monday/goap/extractors_generated.c
    monday/goap/intent_table.cpp
    monday/goap/goap_planner.cpp
)
target_include_directories(monday_goap_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(monday_goap_lib PUBLIC monday_core_lib)

# ── monday_bt_lib (Phase 2 — Behavior Tree Engine) ───────────────────────────
add_library(monday_bt_lib STATIC
    monday/bt/bt_engine.cpp
)
target_include_directories(monday_bt_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(monday_bt_lib PUBLIC monday_core_lib monday_goap_lib)

# ── monday_storage_lib (Phase 3 — SQLCipher + Migrations) ────────────────────
add_library(monday_storage_lib STATIC
    monday/storage/db.cpp
    monday/storage/migrations.cpp
    monday/storage/integrity_check.cpp
)
target_include_directories(monday_storage_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(monday_storage_lib PUBLIC
    monday_core_lib
    monday_security_lib
    ${SQLCIPHER_LIBRARIES}
)

# ── monday_engram_lib (Phase 3 — Episodic + Semantic + Summariser) ───────────
add_library(monday_engram_lib STATIC
    monday/engram/episodic_store.cpp
    monday/engram/semantic_store.cpp
    monday/engram/summariser.cpp
    monday/engram/engram.cpp
)
target_include_directories(monday_engram_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(monday_engram_lib PUBLIC
    monday_core_lib
    monday_storage_lib
    monday_inference_lib
)

# ── monday_rag_lib (Phase 3 — Indexing + Ingestion + Hybrid) ─────────────────
add_library(monday_rag_lib STATIC
    monday/rag/embedding/minilm_embedder.cpp
    monday/rag/backends/mrl_index.cpp
    monday/rag/backends/diskann_index.cpp
    monday/rag/tier_selector.cpp
    monday/rag/ingestion_pipeline.cpp
    monday/rag/hybrid_retriever.cpp
)
target_include_directories(monday_rag_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(monday_rag_lib PUBLIC
    monday_core_lib
    monday_storage_lib
    monday_engram_lib
)

# ── monday_automation_lib (Phase 4 — PID Verifier + Blocklist + Drivers + Dispatcher) ──
add_library(monday_automation_lib STATIC
    monday/automation/automation_blocklist.c
    monday/automation/pid_verifier.cpp
    monday/automation/linux/atspi_driver.cpp
    monday/automation/linux/libei_input.cpp
    monday/automation/linux/libxdo_input.cpp
    monday/automation/windows/uia_driver.cpp
    monday/automation/tool_dispatcher.cpp
)
target_include_directories(monday_automation_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(monday_automation_lib PUBLIC
    monday_core_lib
    monday_security_lib
)

# ── monday_parsers_lib (Phase 4 — Zip, PDF, DOCX, OCR & Sandboxed Subprocess) ──
add_library(monday_parsers_lib STATIC
    monday/parsers/zip_parser.cpp
    monday/parsers/pdf_parser.cpp
    monday/parsers/docx_parser.cpp
    monday/parsers/ocr_tesseract.cpp
    monday/parsers/parser_subprocess.cpp
)
target_include_directories(monday_parsers_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(monday_parsers_lib PUBLIC
    monday_core_lib
    monday_security_lib
)

# ── monday-trusted-helper (Phase 4 — Passphrase, Consent & Cherry Helper GUI/IPC) ──
add_executable(monday-trusted-helper
    trusted-helper/main.cpp
    trusted-helper/passphrase_dialog.cpp
    trusted-helper/consent_dialog.cpp
    trusted-helper/cherry_dialog.cpp
)
target_include_directories(monday-trusted-helper PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(monday-trusted-helper PRIVATE
    monday_core_lib
    monday_security_lib
)

# ── monday-core executable ────────────────────────────────────────────────────
add_executable(monday-core
    monday-core/main.cpp
    monday-core/main_loop.cpp
    monday-core/shutdown.cpp
)
target_include_directories(monday-core PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/generated
)
target_link_libraries(monday-core PRIVATE
    monday_core_lib
    monday_memory_lib
    monday_security_lib
    monday_inference_lib
    monday_voice_lib
    monday_goap_lib
    monday_bt_lib
    monday_storage_lib
    monday_engram_lib
    monday_rag_lib
    monday_automation_lib
    monday_parsers_lib
    pthread
)
add_dependencies(monday-core generate_blackboard)
add_dependencies(monday-core generate_blackboard)

# ── monday-sandbox executable ─────────────────────────────────────────────────
add_executable(monday-sandbox
    tools/interactive_sandbox.cpp
)
target_include_directories(monday-sandbox PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/generated
)
target_link_libraries(monday-sandbox PRIVATE
    monday_core_lib
    monday_memory_lib
    monday_security_lib
    monday_inference_lib
    monday_storage_lib
    monday_engram_lib
    monday_rag_lib
    pthread
)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
