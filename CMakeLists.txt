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

# ── monday_core_lib (shared across tests and binary) ─────────────────────────
add_library(monday_core_lib STATIC
    monday/core/log.c
    src/generated/blackboard_accessors.c
)
add_dependencies(monday_core_lib generate_blackboard)
target_include_directories(monday_core_lib PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/generated)
target_link_libraries(monday_core_lib PUBLIC tl_expected)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
