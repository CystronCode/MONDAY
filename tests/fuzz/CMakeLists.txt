# tests/fuzz/CMakeLists.txt

# Helper to add a fuzz target
function(add_monday_fuzz_target name source_file)
    add_executable(${name} ${source_file} ${ARGN})
    
    # Configure compiler and linker flags for libFuzzer
    target_compile_options(${name} PRIVATE -fsanitize=fuzzer)
    target_link_options(${name} PRIVATE -fsanitize=fuzzer)
    
    # Ensure outputs are stored in build/fuzz
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fuzz"
    )
    
    # Include paths
    target_include_directories(${name} PRIVATE 
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/generated
    )
endfunction()

# ── fuzz_ipc_decoder ──────────────────────────────────────────────────────────
add_monday_fuzz_target(fuzz_ipc_decoder fuzz_ipc_decoder.cpp 
    ${CMAKE_SOURCE_DIR}/src/generated/monday_ipc_v1.pb.cc
)
add_dependencies(fuzz_ipc_decoder generate_proto)
target_link_libraries(fuzz_ipc_decoder PRIVATE 
    monday_core_lib 
    ${Protobuf_LIBRARIES}
)

# ── fuzz_intent_parser ────────────────────────────────────────────────────────
add_monday_fuzz_target(fuzz_intent_parser fuzz_intent_parser.cpp)
add_dependencies(fuzz_intent_parser generate_blackboard)
target_link_libraries(fuzz_intent_parser PRIVATE 
    monday_core_lib 
    monday_goap_lib
)

# ── fuzz_pdf_parser ───────────────────────────────────────────────────────────
add_monday_fuzz_target(fuzz_pdf_parser fuzz_pdf_parser.cpp)
target_link_libraries(fuzz_pdf_parser PRIVATE 
    monday_core_lib 
    monday_parsers_lib
)

# ── fuzz_zip_parser ───────────────────────────────────────────────────────────
add_monday_fuzz_target(fuzz_zip_parser fuzz_zip_parser.cpp)
target_link_libraries(fuzz_zip_parser PRIVATE 
    monday_core_lib 
    monday_parsers_lib
)

# ── fuzz_docx_parser ──────────────────────────────────────────────────────────
add_monday_fuzz_target(fuzz_docx_parser fuzz_docx_parser.cpp)
target_link_libraries(fuzz_docx_parser PRIVATE 
    monday_core_lib 
    monday_parsers_lib
)
