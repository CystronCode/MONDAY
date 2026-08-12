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
target_link_libraries(monday_memory_lib PUBLIC monday_core_lib)

# ── monday_security_lib (Phase 1 — Ed25519 + model_verify) ───────────────────
add_library(monday_security_lib STATIC
    monday/security/ed25519_verify.c
    monday/security/model_verify.cpp
)
target_include_directories(monday_security_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(monday_security_lib PUBLIC monday_core_lib)
# Uncomment when libsodium is available:
# find_package(sodium REQUIRED)
# target_link_libraries(monday_security_lib PUBLIC sodium)
# target_compile_definitions(monday_security_lib PUBLIC MONDAY_HAVE_LIBSODIUM)

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
target_include_directories(monday_inference_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(monday_inference_lib PUBLIC
    monday_core_lib
    monday_memory_lib
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
    pthread
)
add_dependencies(monday-core generate_blackboard)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
