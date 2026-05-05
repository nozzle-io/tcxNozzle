add_executable(headless_test ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_nozzle.cpp)
target_include_directories(headless_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
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
