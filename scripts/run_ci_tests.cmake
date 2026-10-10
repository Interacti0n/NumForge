# Keep server tests serial, and run the exhaustive allocation shards on two
# workers. Both groups run even when one fails, so CI reports all failures.
if(NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "Pass -DBUILD_DIR=<configured build directory>")
endif()
if(NOT DEFINED CONFIGURATION)
    set(CONFIGURATION RelWithDebInfo)
endif()
execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${BUILD_DIR}"
    -C "${CONFIGURATION}" -LE allocation --output-on-failure --no-tests=error
    RESULT_VARIABLE ordinary_result)
execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${BUILD_DIR}"
    -C "${CONFIGURATION}" -L allocation --parallel 2 --output-on-failure --no-tests=error
    RESULT_VARIABLE allocation_result)
if(NOT ordinary_result STREQUAL "0" OR NOT allocation_result STREQUAL "0")
    message(FATAL_ERROR "Tests failed (ordinary=${ordinary_result}, allocation=${allocation_result})")
endif()
