# Builds the headless functional test (test_nozzle.cpp) as a second executable
# alongside the host app, and runs it after every build.
add_executable(headless_test ${CMAKE_CURRENT_SOURCE_DIR}/test_nozzle.cpp)
target_link_libraries(headless_test PRIVATE tcxNozzle)
set_target_properties(headless_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND headless_test
    COMMENT "Running headless tests..."
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Throwaway windowed GPU round-trip verifier (manual run, not auto-run).
add_executable(gpu_roundtrip ${CMAKE_CURRENT_SOURCE_DIR}/gpu_roundtrip.cpp)
target_link_libraries(gpu_roundtrip PRIVATE tcxNozzle)
set_target_properties(gpu_roundtrip PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
