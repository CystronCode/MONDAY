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

# ── Phase 2: test_spsc_ring (C) ───────────────────────────────────────────────
add_executable(test_spsc_ring test_spsc_ring.c)
target_link_libraries(test_spsc_ring PRIVATE monday_core_lib monday_memory_lib monday_voice_lib unity pthread)
target_include_directories(test_spsc_ring PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_spsc_ring COMMAND test_spsc_ring)

# ── Phase 2: test_bandpass_filter (C) ─────────────────────────────────────────
add_executable(test_bandpass_filter test_bandpass_filter.c)
target_link_libraries(test_bandpass_filter PRIVATE monday_core_lib monday_memory_lib monday_voice_lib unity m)
target_include_directories(test_bandpass_filter PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_bandpass_filter COMMAND test_bandpass_filter)

# ── Phase 2: test_goap (C++) ──────────────────────────────────────────────────
add_executable(test_goap test_goap.cpp)
target_link_libraries(test_goap PRIVATE Catch2::Catch2WithMain monday_core_lib monday_memory_lib monday_goap_lib)
target_include_directories(test_goap PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_goap COMMAND test_goap)

# ── Phase 2: test_bt_engine (C++) ─────────────────────────────────────────────
add_executable(test_bt_engine test_bt_engine.cpp)
target_link_libraries(test_bt_engine PRIVATE Catch2::Catch2WithMain monday_core_lib monday_memory_lib monday_goap_lib monday_bt_lib)
target_include_directories(test_bt_engine PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/generated)
add_test(NAME test_bt_engine COMMAND test_bt_engine)
