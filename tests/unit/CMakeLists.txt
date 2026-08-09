# tests/unit/CMakeLists.txt

# ── test_error_taxonomy (C++) ─────────────────────────────────────────────────
add_executable(test_error_taxonomy test_error_taxonomy.cpp)
target_link_libraries(test_error_taxonomy PRIVATE Catch2::Catch2WithMain monday_core_lib)
target_include_directories(test_error_taxonomy PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_error_taxonomy COMMAND test_error_taxonomy)

# ── test_logger (C) ───────────────────────────────────────────────────────────
add_executable(test_logger test_logger.c)
target_link_libraries(test_logger PRIVATE monday_core_lib unity pthread)
target_include_directories(test_logger PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_logger COMMAND test_logger)
