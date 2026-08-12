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

# ── Phase 1: test_allocator (C) ───────────────────────────────────────────────
add_executable(test_allocator test_allocator.c)
target_link_libraries(test_allocator PRIVATE
    monday_core_lib
    monday_memory_lib
    unity
    pthread
)
target_include_directories(test_allocator PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_allocator COMMAND test_allocator)

# ── Phase 1: test_epoch (C) ───────────────────────────────────────────────────
add_executable(test_epoch test_epoch.c)
target_link_libraries(test_epoch PRIVATE
    monday_core_lib
    monday_memory_lib
    unity
    pthread
)
target_include_directories(test_epoch PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_epoch COMMAND test_epoch)
