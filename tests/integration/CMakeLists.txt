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

# ── Phase 2: test_voice_pipeline (C++) ───────────────────────────────────────
add_executable(test_voice_pipeline test_voice_pipeline.cpp)
target_link_libraries(test_voice_pipeline PRIVATE
    Catch2::Catch2WithMain
    monday_core_lib
    monday_memory_lib
    monday_voice_lib
    monday_goap_lib
    monday_bt_lib
    pthread
)
target_include_directories(test_voice_pipeline PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/generated
)
add_test(NAME test_voice_pipeline COMMAND test_voice_pipeline)

# ── Phase 3: test_rag_pipeline (C++) ─────────────────────────────────────────
add_executable(test_rag_pipeline test_rag_pipeline.cpp)
target_link_libraries(test_rag_pipeline PRIVATE
    Catch2::Catch2WithMain
    monday_core_lib
    monday_memory_lib
    monday_security_lib
    monday_storage_lib
    monday_engram_lib
    monday_rag_lib
    pthread
)
target_include_directories(test_rag_pipeline PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/generated
)
add_test(NAME test_rag_pipeline COMMAND test_rag_pipeline)

# ── Phase 5: test_plugin_lifecycle (C++) ─────────────────────────────────────
add_executable(test_plugin_lifecycle test_plugin_lifecycle.cpp)
target_link_libraries(test_plugin_lifecycle PRIVATE
    Catch2::Catch2WithMain
    monday_core_lib
    monday_memory_lib
    monday_security_lib
    monday_plugin_lib
    ${LUA_LIBRARIES}
    ${SODIUM_LIBRARIES}
    pthread
)
target_include_directories(test_plugin_lifecycle PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/generated
)
add_test(NAME test_plugin_lifecycle COMMAND test_plugin_lifecycle)


