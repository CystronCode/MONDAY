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

# ── Phase 3: test_kdf (C++) ──────────────────────────────────────────────────
add_executable(test_kdf test_kdf.cpp)
target_link_libraries(test_kdf PRIVATE Catch2::Catch2WithMain monday_core_lib monday_security_lib)
target_include_directories(test_kdf PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_kdf COMMAND test_kdf)

# ── Phase 3: test_audit_log (C++) ────────────────────────────────────────────
add_executable(test_audit_log test_audit_log.cpp)
target_link_libraries(test_audit_log PRIVATE Catch2::Catch2WithMain monday_core_lib monday_security_lib)
target_include_directories(test_audit_log PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_audit_log COMMAND test_audit_log)

# ── Phase 3: test_engram (C++) ───────────────────────────────────────────────
add_executable(test_engram test_engram.cpp)
target_link_libraries(test_engram PRIVATE Catch2::Catch2WithMain monday_core_lib monday_memory_lib monday_security_lib monday_storage_lib monday_engram_lib monday_inference_lib)
target_include_directories(test_engram PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_engram COMMAND test_engram)

# ── Phase 3: test_hybrid_retriever (C++) ─────────────────────────────────────
add_executable(test_hybrid_retriever test_hybrid_retriever.cpp)
target_link_libraries(test_hybrid_retriever PRIVATE Catch2::Catch2WithMain monday_core_lib monday_memory_lib monday_security_lib monday_storage_lib monday_engram_lib monday_rag_lib)
target_include_directories(test_hybrid_retriever PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_hybrid_retriever COMMAND test_hybrid_retriever)

# ── Phase 3: test_tier_selector (C++) ────────────────────────────────────────
add_executable(test_tier_selector test_tier_selector.cpp)
target_link_libraries(test_tier_selector PRIVATE Catch2::Catch2WithMain monday_core_lib monday_memory_lib monday_security_lib monday_storage_lib monday_engram_lib monday_rag_lib)
target_include_directories(test_tier_selector PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_tier_selector COMMAND test_tier_selector)

# ── Phase 4: test_consent (C++) ──────────────────────────────────────────────
add_executable(test_consent test_consent.cpp)
target_link_libraries(test_consent PRIVATE Catch2::Catch2WithMain monday_core_lib monday_security_lib ${SQLCIPHER_LIBRARIES})
target_include_directories(test_consent PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_consent COMMAND test_consent)

# ── Phase 4: test_pid_verifier (C++) ─────────────────────────────────────────
add_executable(test_pid_verifier test_pid_verifier.cpp)
target_link_libraries(test_pid_verifier PRIVATE Catch2::Catch2WithMain monday_core_lib monday_security_lib monday_automation_lib)
target_include_directories(test_pid_verifier PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_pid_verifier COMMAND test_pid_verifier)

# ── Phase 4: test_zip_parser (C++) ────────────────────────────────────────────
add_executable(test_zip_parser test_zip_parser.cpp)
target_link_libraries(test_zip_parser PRIVATE Catch2::Catch2WithMain monday_core_lib monday_security_lib monday_parsers_lib)
target_include_directories(test_zip_parser PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_zip_parser COMMAND test_zip_parser)

# ── Phase 5: test_plugin_loader (C++) ─────────────────────────────────────────
add_executable(test_plugin_loader test_plugin_loader.cpp)
target_link_libraries(test_plugin_loader PRIVATE Catch2::Catch2WithMain monday_core_lib monday_security_lib monday_plugin_lib ${SODIUM_LIBRARIES})
target_include_directories(test_plugin_loader PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_plugin_loader COMMAND test_plugin_loader)



