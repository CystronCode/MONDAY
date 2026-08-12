# tests/integration/CMakeLists.txt
#
# Phase 1 integration tests.

# ── test_inference_engine (C++) ───────────────────────────────────────────────

add_executable(test_inference_engine
    test_inference_engine.cpp
)
target_link_libraries(test_inference_engine PRIVATE
    Catch2::Catch2WithMain
    monday_core_lib
    monday_inference_lib
    monday_security_lib
    monday_memory_lib
    pthread
)
target_include_directories(test_inference_engine PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/generated
)
add_test(NAME test_inference_engine COMMAND test_inference_engine)
