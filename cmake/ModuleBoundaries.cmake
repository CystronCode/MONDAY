# cmake/ModuleBoundaries.cmake  (stub — enforced more strictly in Phase 6)
# Rule: no Python extension modules may be linked into any runtime target.
# This file will gain CMake checks in Phase 6 that verify no .py is
# referenced by any non-tooling target. For now it documents the contract.
message(STATUS "ModuleBoundaries: Python excluded from runtime targets (Phase 0 stub)")
