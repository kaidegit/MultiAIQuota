enable_testing()

# Simple executable tests (no external test framework for now)
add_executable(maiq_core_tests test_providers.cpp)

target_link_libraries(maiq_core_tests
    PRIVATE
        maiq_core
        maiq_platform_pc
)

target_include_directories(maiq_core_tests PRIVATE ${CMAKE_SOURCE_DIR}/core/include)

add_test(NAME maiq_core_tests COMMAND maiq_core_tests)

# Live integration test: requires tests/live_config.json with real API keys.
# This file is gitignored and never committed.
add_executable(maiq_integration_tests test_integration.cpp)

target_link_libraries(maiq_integration_tests
    PRIVATE
        maiq_core
        maiq_platform_pc
)

target_include_directories(maiq_integration_tests PRIVATE ${CMAKE_SOURCE_DIR}/core/include)
