cmake_minimum_required(VERSION 3.20)

project(NumForge
    VERSION 2.2.0
    DESCRIPTION "Arbitrary-precision mathematics library and exact-decimal calculator written in C"
    LANGUAGES C
)

# Keep a compile database available to IDEs such as VS Code IntelliSense.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(NUMFORGE_WARNINGS_AS_ERRORS
    "Treat warnings in NumForge sources as errors"
    OFF
)
option(NUMFORGE_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer where supported"
    OFF
)

include(CTest)

option(NUMFORGE_BUILD_APPS "Build and install CLI and local web applications" ON)

option(NUMFORGE_BUILD_BENCHMARKS "Build opt-in performance benchmarks" OFF)
option(NUMFORGE_REQUIRE_NODE_TESTS "Fail test configuration if Node.js is unavailable" OFF)
option(NUMFORGE_ENABLE_COVERAGE "Instrument a GCC test build for gcov reports" OFF)
option(NUMFORGE_BUILD_FUZZERS "Build Clang libFuzzer parser and number targets" OFF)

if(NUMFORGE_ENABLE_COVERAGE)
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" OR NOT BUILD_TESTING OR NUMFORGE_ENABLE_SANITIZERS)
        message(FATAL_ERROR "Coverage requires a separate GCC BUILD_TESTING build without sanitizers")
    endif()
    find_program(NUMFORGE_GCOV NAMES gcov REQUIRED)
    add_custom_target(coverage_report
        COMMAND ${CMAKE_COMMAND} "-DBUILD_DIR=${PROJECT_BINARY_DIR}"
            "-DGCOV=${NUMFORGE_GCOV}" -P "${PROJECT_SOURCE_DIR}/cmake/Coverage.cmake"
        COMMENT "Collecting gcov line and branch reports after CTest")
endif()

if(NUMFORGE_BUILD_FUZZERS AND (NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR WIN32 OR NUMFORGE_ENABLE_COVERAGE))
    message(FATAL_ERROR "libFuzzer profile requires a separate Unix Clang build")
endif()

function(numforge_enable_diagnostics target)
    if(NUMFORGE_BUILD_BENCHMARKS AND NOT "${target}" STREQUAL "web_server_smoke_tests")
        target_compile_definitions(${target} PRIVATE NUMFORGE_ENABLE_ALLOC_STATS)
        if(APPLE)
            # Forced stdlib includes can lock libc feature selection before
            # source-local clock_gettime declarations request POSIX support.
            target_compile_definitions(${target} PRIVATE _DARWIN_C_SOURCE)
        elseif(NOT WIN32)
            target_compile_definitions(${target} PRIVATE _POSIX_C_SOURCE=200809L)
        endif()
        if(MSVC)
            # The forced include loads stdlib before source-local CRT defines.
            target_compile_definitions(${target} PRIVATE "_CRT_SECURE_NO_WARNINGS=")
            target_compile_options(${target} PRIVATE
                "/FI${PROJECT_SOURCE_DIR}/src/internal/benchmark_redirect.h")
        else()
            target_compile_options(${target} PRIVATE -include
                "${PROJECT_SOURCE_DIR}/src/internal/benchmark_redirect.h")
        endif()
    endif()
    if(NUMFORGE_ENABLE_COVERAGE)
        target_compile_options(${target} PRIVATE --coverage -fprofile-abs-path -O0 -g)
        target_link_options(${target} PRIVATE --coverage)
    endif()
    if(NUMFORGE_BUILD_FUZZERS)
        target_compile_options(${target} PRIVATE -fsanitize=fuzzer-no-link)
        target_link_options(${target} PRIVATE -fsanitize=fuzzer-no-link)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
        if(NUMFORGE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(NUMFORGE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    if(NUMFORGE_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

# ============================================================
# NumForge library
# ============================================================

add_library(numforge STATIC
    src/bigint/bigint.c
    src/bigint/arithmetic.c
    src/bigint/bitwise.c
    src/bigint/comparison.c
    src/bigint/conversion.c
    src/bigint/number_theory.c
    src/bigint/primality.c
    src/bigint/roots.c
    src/bigdecimal/bigdecimal.c
    src/bigdecimal/arithmetic.c
    src/bigdecimal/aggregation.c
    src/bigdecimal/statistics.c
    src/bigdecimal/comparison.c
    src/bigdecimal/conversion.c
    src/bigdecimal/division.c
    src/bigdecimal/operations.c
    src/bigdecimal/roots.c
    src/bigdecimal/transcendental.c
    src/bigdecimal/hyperbolic.c
    src/bigdecimal/trigonometric.c
    src/bigdecimal/format.c
    src/bigdecimal/format_mode.c
    src/bigdecimal/constants.c
    src/bigrational/bigrational.c
    src/bigcomplex/bigcomplex.c
    src/bigcomplex/bigrationalcomplex.c
    src/units/units.c
    src/internal/numforge_alloc.c
)

# Private expression engine, application state and HTTP adapters. None are
# exported or installed as the public numeric API.
if(NUMFORGE_BUILD_APPS OR BUILD_TESTING OR NUMFORGE_BUILD_FUZZERS OR NUMFORGE_BUILD_BENCHMARKS)
    add_library(numforge_calculator STATIC
        src/calculator/calculator.c
        src/calculator/value.c
        src/calculator/complex_evaluator.c
        src/calculator/quantity.c
        src/calculator/random.c
        src/calculator/constants.c
        src/calculator/tokenizer.c
        src/calculator/parser.c
        src/calculator/evaluator.c
        src/calculator/evaluator_functions.c
        src/calculator/evaluator_trigonometric.c
        src/calculator/exact_evaluator.c
        src/calculator/exact_functions.c
        src/calculator/functions.c
        src/calculator/formatter.c
    )
    target_link_libraries(numforge_calculator PUBLIC numforge)
    if(WIN32)
        target_link_libraries(numforge_calculator PRIVATE bcrypt)
    endif()
    target_include_directories(numforge_calculator PUBLIC ${PROJECT_SOURCE_DIR}/src/calculator)
    numforge_enable_diagnostics(numforge_calculator)

    add_library(numforge_application STATIC
        src/application/conversion.c
        src/application/session.c
        src/application/client_store.c)
    target_link_libraries(numforge_application PUBLIC numforge_calculator)
    target_include_directories(numforge_application PUBLIC ${PROJECT_SOURCE_DIR}/src/application)
    numforge_enable_diagnostics(numforge_application)

    add_library(numforge_client STATIC
        src/web/session_api.c
        src/web/web_api.c
        src/web/unit_web.c
        src/web/http_request.c)
    target_link_libraries(numforge_client PUBLIC numforge_application)
    target_include_directories(numforge_client PUBLIC
        ${PROJECT_SOURCE_DIR}/src/web)
    numforge_enable_diagnostics(numforge_client)
endif()

add_library(NumForge::numforge ALIAS numforge)

target_include_directories(numforge
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

numforge_enable_diagnostics(numforge)

if(BUILD_TESTING)
    target_compile_definitions(numforge PRIVATE
        NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
    )
endif()

# ============================================================
# Calculator application
# ============================================================

if(NUMFORGE_BUILD_APPS)
add_executable(calculator
    src/main.c
)

target_include_directories(calculator
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/calculator
)

target_link_libraries(calculator
    PRIVATE
        numforge_application
)
numforge_enable_diagnostics(calculator)

# ============================================================
# Local web application
# ============================================================

set(NUMFORGE_WEB_ASSETS
    LICENSE
    web/calculator.sk.html
    web/calculator.en.html
    web/api.sk.html
    web/api.en.html
    web/upcoming.sk.html
    web/upcoming.en.html
    web/calculator.css
    web/api.css
    web/chrome.css
    web/navigation.js
    web/calculator.js
    web/units.sk.html
    web/units.en.html
    web/units.css
    web/units.js
    web/logo.png
    web/wordmark.png
)
set(NUMFORGE_WEB_GENERATED_DIR "${PROJECT_BINARY_DIR}/generated/web")
set(NUMFORGE_WEB_GENERATED_HEADER "${NUMFORGE_WEB_GENERATED_DIR}/web_page.h")
add_custom_command(
    OUTPUT "${NUMFORGE_WEB_GENERATED_HEADER}"
    COMMAND "${CMAKE_COMMAND}"
        "-DNUMFORGE_WEB_SOURCE_DIR=${PROJECT_SOURCE_DIR}/web"
        "-DNUMFORGE_WEB_OUTPUT=${NUMFORGE_WEB_GENERATED_HEADER}"
        "-DNUMFORGE_WEB_VERSION=${PROJECT_VERSION}"
        -P "${PROJECT_SOURCE_DIR}/cmake/EmbedWeb.cmake"
    DEPENDS "${PROJECT_SOURCE_DIR}/cmake/EmbedWeb.cmake"
        ${NUMFORGE_WEB_ASSETS}
    VERBATIM
)

add_executable(numforge_web
    src/web/web_main.c
    src/web/web_server.c
    "${NUMFORGE_WEB_GENERATED_HEADER}"
)

target_include_directories(numforge_web
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/calculator
        ${PROJECT_SOURCE_DIR}/src/web
        ${NUMFORGE_WEB_GENERATED_DIR}
)

target_link_libraries(numforge_web
    PRIVATE
        numforge_client
)

if(WIN32)
    target_link_libraries(numforge_web PRIVATE ws2_32 shell32)
endif()

numforge_enable_diagnostics(numforge_web)
endif()

# ============================================================
# Installation and CMake package
# ============================================================

set(NUMFORGE_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/NumForge"
)

install(
    TARGETS numforge
    EXPORT NumForgeTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(NUMFORGE_BUILD_APPS)
    install(
        TARGETS calculator numforge_web
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(
    DIRECTORY include/numforge
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(
    FILES LICENSE
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/NumForge"
)

install(
    FILES README.md CHANGELOG.md CHANGELOG_SHORT.md LICENSE
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/NumForge"
)

install(
    DIRECTORY docs/
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/NumForge/docs"
    FILES_MATCHING PATTERN "*.md" PATTERN "*.png"
)

configure_package_config_file(
    cmake/NumForgeConfig.cmake.in
    "${PROJECT_BINARY_DIR}/NumForgeConfig.cmake"
    INSTALL_DESTINATION "${NUMFORGE_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/NumForgeConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(
    EXPORT NumForgeTargets
    FILE NumForgeTargets.cmake
    NAMESPACE NumForge::
    DESTINATION "${NUMFORGE_INSTALL_CMAKEDIR}"
)

install(
    FILES
        "${PROJECT_BINARY_DIR}/NumForgeConfig.cmake"
        "${PROJECT_BINARY_DIR}/NumForgeConfigVersion.cmake"
    DESTINATION "${NUMFORGE_INSTALL_CMAKEDIR}"
)

# ============================================================
# Tests
# ============================================================

if(NUMFORGE_BUILD_BENCHMARKS)
    target_sources(numforge PRIVATE src/internal/benchmark_profile.c src/internal/decimal_profile.c)
    if(WIN32)
        target_link_libraries(numforge PRIVATE psapi)
    elseif(APPLE)
        target_compile_definitions(numforge PRIVATE _DARWIN_C_SOURCE)
    else()
        target_compile_definitions(numforge PRIVATE _POSIX_C_SOURCE=200809L)
    endif()
    add_executable(allocation_stats_check benchmarks/allocation_stats_check.c)
    target_link_libraries(allocation_stats_check PRIVATE numforge)
    numforge_enable_diagnostics(allocation_stats_check)
    add_executable(cache_memory_benchmark benchmarks/cache_memory_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(cache_memory_benchmark PRIVATE numforge_client)
    numforge_enable_diagnostics(cache_memory_benchmark)
    if(BUILD_TESTING)
        add_test(NAME allocation_stats_check COMMAND allocation_stats_check)
        add_test(NAME cache_memory_benchmark_smoke COMMAND cache_memory_benchmark --quick)
        add_test(NAME decimal_format_benchmark_smoke COMMAND decimal_format_benchmark --quick)
    endif()
    target_compile_definitions(numforge PRIVATE NUMFORGE_ENABLE_ALLOC_STATS)
    add_executable(calculator_benchmark benchmarks/calculator_benchmark.c benchmarks/benchmark_clock.c)
    target_compile_definitions(calculator_benchmark PRIVATE NUMFORGE_ENABLE_ALLOC_STATS)
    target_include_directories(calculator_benchmark PRIVATE ${PROJECT_SOURCE_DIR}/src/calculator)
    target_link_libraries(calculator_benchmark PRIVATE numforge_client)
    numforge_enable_diagnostics(calculator_benchmark)
    add_executable(bigint_multiply_benchmark benchmarks/bigint_multiply_benchmark.c benchmarks/benchmark_clock.c)
    target_compile_definitions(bigint_multiply_benchmark PRIVATE NUMFORGE_ENABLE_ALLOC_STATS)
    target_link_libraries(bigint_multiply_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(bigint_multiply_benchmark)
    add_executable(bigint_arithmetic_benchmark benchmarks/bigint_arithmetic_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(bigint_arithmetic_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(bigint_arithmetic_benchmark)
    add_executable(decimal_math_benchmark benchmarks/decimal_math_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(decimal_math_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(decimal_math_benchmark)
    if(BUILD_TESTING)
        target_compile_definitions(bigint_arithmetic_benchmark PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
        find_program(NUMFORGE_BIGINT_BENCH_NODE NAMES node nodejs)
        if(NUMFORGE_BIGINT_BENCH_NODE)
            add_test(NAME bigint_arithmetic_benchmark_smoke
                COMMAND ${NUMFORGE_BIGINT_BENCH_NODE} ${PROJECT_SOURCE_DIR}/benchmarks/check_bigint_benchmark.js $<TARGET_FILE:bigint_arithmetic_benchmark>)
            add_test(NAME decimal_math_benchmark_smoke
                COMMAND ${NUMFORGE_BIGINT_BENCH_NODE} ${PROJECT_SOURCE_DIR}/benchmarks/check_decimal_math_benchmark.js $<TARGET_FILE:decimal_math_benchmark>)
            set_tests_properties(decimal_math_benchmark_smoke PROPERTIES TIMEOUT 120)
        elseif(NUMFORGE_REQUIRE_NODE_TESTS)
            message(FATAL_ERROR "BigInt benchmark validation requires Node.js")
        endif()
    endif()
    add_executable(decimal_format_benchmark benchmarks/decimal_format_benchmark.c benchmarks/benchmark_clock.c)
    target_compile_definitions(decimal_format_benchmark PRIVATE NUMFORGE_ENABLE_ALLOC_STATS)
    target_link_libraries(decimal_format_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(decimal_format_benchmark)
endif()

if(NUMFORGE_BUILD_FUZZERS)
    foreach(kind IN ITEMS parser numbers formatter http)
        add_executable(fuzz_${kind} tests/fuzz/fuzz_${kind}.c)
        target_include_directories(fuzz_${kind} PRIVATE ${PROJECT_SOURCE_DIR}/src/calculator)
        target_link_libraries(fuzz_${kind} PRIVATE numforge_client)
        numforge_enable_diagnostics(fuzz_${kind})
        target_link_options(fuzz_${kind} PRIVATE -fsanitize=fuzzer)
    endforeach()
endif()

if(BUILD_TESTING)
    include(FetchContent)

    foreach(kind IN ITEMS parser numbers formatter http)
        add_executable(fuzz_${kind}_smoke tests/fuzz/fuzz_${kind}.c tests/fuzz/replay.c)
        target_include_directories(fuzz_${kind}_smoke PRIVATE ${PROJECT_SOURCE_DIR}/src/calculator)
        target_link_libraries(fuzz_${kind}_smoke PRIVATE numforge_client)
        numforge_enable_diagnostics(fuzz_${kind}_smoke)
        add_test(NAME fuzz_${kind}_smoke COMMAND fuzz_${kind}_smoke)
        set_tests_properties(fuzz_${kind}_smoke PROPERTIES TIMEOUT 60)
    endforeach()

    FetchContent_Declare(
        unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        # Exact v2.6.1 commit for reproducible test dependency resolution.
        GIT_TAG cbcd08fa7de711053a3deec6339ee89cad5d2697
    )

    FetchContent_MakeAvailable(unity)

    # NumForge tests exercise int64_t calculator settings and uint64_t-backed
    # counters even when the test executable itself is built for 32 bits.
    target_compile_definitions(unity PUBLIC UNITY_SUPPORT_64)

    # Unity is a test-only dependency. Excluding its source directory keeps
    # its own install rules out of NumForge installation packages.
    set_property(DIRECTORY "${unity_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)

    add_executable(bigint_tests
        tests/test_bigint.c
    )

    add_executable(public_api_tests tests/test_public_api.c)
    target_link_libraries(public_api_tests PRIVATE numforge unity)
    numforge_enable_diagnostics(public_api_tests)
    add_test(NAME public_api_tests COMMAND public_api_tests)

    target_link_libraries(bigint_tests
        PRIVATE
        numforge
        unity
    )
    numforge_enable_diagnostics(bigint_tests)

    add_test(
        NAME bigint_tests
        COMMAND bigint_tests
    )

    add_executable(bigint_property_tests
        tests/test_bigint_properties.c
    )

    target_link_libraries(bigint_property_tests
        PRIVATE
        numforge
        unity
    )
    numforge_enable_diagnostics(bigint_property_tests)

    add_test(
        NAME bigint_property_tests
        COMMAND bigint_property_tests
    )

    add_executable(bigdecimal_tests
        tests/test_bigdecimal.c
    )

    add_executable(bigrational_tests tests/test_bigrational.c)
    target_link_libraries(bigrational_tests PRIVATE numforge unity)
    numforge_enable_diagnostics(bigrational_tests)
    add_test(NAME bigrational_tests COMMAND bigrational_tests)

    add_executable(bigcomplex_tests tests/test_bigcomplex.c)
    target_link_libraries(bigcomplex_tests PRIVATE numforge unity)
    target_include_directories(bigcomplex_tests PRIVATE src/internal)
    target_compile_definitions(bigcomplex_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    numforge_enable_diagnostics(bigcomplex_tests)
    add_test(NAME bigcomplex_tests COMMAND bigcomplex_tests)
    add_executable(complex_oracle_driver tests/complex_oracle_driver.c benchmarks/benchmark_clock.c)
    target_link_libraries(complex_oracle_driver PRIVATE numforge)
    numforge_enable_diagnostics(complex_oracle_driver)
    find_program(NUMFORGE_COMPLEX_NODE NAMES node nodejs REQUIRED)
    add_test(NAME complex_oracle_tests COMMAND "${NUMFORGE_COMPLEX_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/test_complex_oracle.js" $<TARGET_FILE:complex_oracle_driver>)
    set_tests_properties(complex_oracle_tests PROPERTIES TIMEOUT 150)
    if(NUMFORGE_BUILD_BENCHMARKS)
        add_test(NAME complex_memory_smoke COMMAND "${NUMFORGE_COMPLEX_NODE}"
            "${PROJECT_SOURCE_DIR}/tests/test_complex_oracle.js" $<TARGET_FILE:complex_oracle_driver> --memory --quick)
        set_tests_properties(complex_memory_smoke PROPERTIES TIMEOUT 150)
    endif()

    add_executable(units_tests tests/test_units.c)
    target_link_libraries(units_tests PRIVATE numforge unity)
    target_include_directories(units_tests PRIVATE src/internal)
    target_compile_definitions(units_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    numforge_enable_diagnostics(units_tests)
    add_test(NAME units_tests COMMAND units_tests)

    target_link_libraries(bigdecimal_tests
        PRIVATE
        numforge
        unity
    )
    numforge_enable_diagnostics(bigdecimal_tests)

    add_test(
        NAME bigdecimal_tests
        COMMAND bigdecimal_tests
    )

    add_executable(bigdecimal_property_tests
        tests/test_bigdecimal_properties.c
    )

    target_link_libraries(bigdecimal_property_tests
        PRIVATE
        numforge
        unity
    )
    numforge_enable_diagnostics(bigdecimal_property_tests)

    add_test(
        NAME bigdecimal_property_tests
        COMMAND bigdecimal_property_tests
    )

    add_executable(calculator_tests
        tests/test_calculator.c
    )

    target_include_directories(calculator_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src/calculator
    )

    target_link_libraries(calculator_tests
        PRIVATE
        numforge_client
        unity
    )
    numforge_enable_diagnostics(calculator_tests)

    add_test(
        NAME calculator_tests
        COMMAND calculator_tests
    )

    foreach(suite IN ITEMS calculator_contract parser_fuzz http_request function_calls roots transcendental trigonometric hyperbolic constants web_cache)
        add_executable(${suite}_tests tests/test_${suite}.c)
        target_include_directories(${suite}_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/calculator)
        target_link_libraries(${suite}_tests PRIVATE numforge_client unity)
        numforge_enable_diagnostics(${suite}_tests)
        add_test(NAME ${suite}_tests COMMAND ${suite}_tests)
        set_tests_properties(${suite}_tests PROPERTIES TIMEOUT 60)
    endforeach()

    add_executable(unit_web_tests tests/test_unit_web.c)
    target_link_libraries(unit_web_tests PRIVATE numforge_client unity)
    target_include_directories(unit_web_tests PRIVATE src/internal)
    target_compile_definitions(unit_web_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    numforge_enable_diagnostics(unit_web_tests)
    add_test(NAME unit_web_tests COMMAND unit_web_tests)

    add_executable(web_api_tests
        tests/test_web_api.c
    )

    target_include_directories(web_api_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src/calculator
            ${PROJECT_SOURCE_DIR}/src/web
    )

    target_link_libraries(web_api_tests
        PRIVATE
            numforge_client
            unity
    )
    numforge_enable_diagnostics(web_api_tests)

    add_test(
        NAME web_api_tests
        COMMAND web_api_tests
    )

    if(NUMFORGE_BUILD_APPS)
    add_executable(web_server_smoke_tests
        tests/test_web_server.c
    )

    if(WIN32)
        target_link_libraries(web_server_smoke_tests PRIVATE ws2_32)
    endif()

    add_dependencies(web_server_smoke_tests numforge_web)
    numforge_enable_diagnostics(web_server_smoke_tests)

    add_test(
        NAME web_server_smoke_tests
        COMMAND web_server_smoke_tests $<TARGET_FILE:numforge_web>
    )

    set_tests_properties(web_server_smoke_tests PROPERTIES TIMEOUT 60)
    endif()

    add_executable(allocation_failure_tests
        tests/test_allocation_failures.c
    )

    add_executable(session_tests tests/test_session.c)
    add_executable(calculator_complex_tests tests/test_calculator_complex.c)
    target_link_libraries(calculator_complex_tests PRIVATE numforge_client unity)
    target_include_directories(calculator_complex_tests PRIVATE src/internal)
    target_compile_definitions(calculator_complex_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    numforge_enable_diagnostics(calculator_complex_tests)
    add_test(NAME calculator_complex_tests COMMAND calculator_complex_tests)
    add_executable(application_tests tests/test_application.c)
    target_link_libraries(application_tests PRIVATE numforge_application unity)
    numforge_enable_diagnostics(application_tests)
    add_test(NAME application_tests COMMAND application_tests)
    target_include_directories(session_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/internal)
    target_compile_definitions(session_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    target_link_libraries(session_tests PRIVATE numforge_client unity)
    numforge_enable_diagnostics(session_tests)
    add_test(NAME session_tests COMMAND session_tests)
    set_tests_properties(session_tests PROPERTIES TIMEOUT 60)

    target_include_directories(allocation_failure_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src/calculator
            ${PROJECT_SOURCE_DIR}/src/internal
            ${PROJECT_SOURCE_DIR}/src/web
    )

    target_compile_definitions(allocation_failure_tests PRIVATE
        NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
    )

    target_link_libraries(allocation_failure_tests
        PRIVATE
            numforge_client
            unity
    )
    numforge_enable_diagnostics(allocation_failure_tests)

    add_test(
        NAME allocation_failure_tests
        COMMAND allocation_failure_tests
    )

    set_tests_properties(
        bigint_tests
        bigint_property_tests
        bigdecimal_tests
        bigdecimal_property_tests
        calculator_tests
        web_api_tests
        allocation_failure_tests
        PROPERTIES TIMEOUT 60
    )

    find_program(NUMFORGE_NODE_EXECUTABLE NAMES node nodejs HINTS "C:/Program Files/nodejs")
    if(NUMFORGE_NODE_EXECUTABLE)
        add_test(NAME unit_catalog_check COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/scripts/generate_unit_catalog.cjs" --check)
        if(NUMFORGE_BUILD_APPS)
        add_test(NAME cli_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/test_cli.js" $<TARGET_FILE:calculator>)
        set_tests_properties(cli_tests PROPERTIES TIMEOUT 60)
        add_test(NAME web_remote_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/test_web_remote.js" $<TARGET_FILE:numforge_web>)
        set_tests_properties(web_remote_tests PROPERTIES TIMEOUT 60)
        endif()
        add_executable(numeric_oracle_driver tests/numeric_oracle_driver.c)
        target_include_directories(numeric_oracle_driver PRIVATE ${PROJECT_SOURCE_DIR}/src/bigdecimal)
        target_link_libraries(numeric_oracle_driver PRIVATE numforge_client)
        numforge_enable_diagnostics(numeric_oracle_driver)
        add_test(NAME numeric_oracle_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/test_numeric_oracle.js" $<TARGET_FILE:numeric_oracle_driver>)
        set_tests_properties(numeric_oracle_tests PROPERTIES TIMEOUT 120)
        add_test(NAME web_ui_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_web_ui.js")
        set_tests_properties(web_ui_tests PROPERTIES TIMEOUT 30)
    else()
        if(NUMFORGE_REQUIRE_NODE_TESTS)
            message(FATAL_ERROR "Node.js is required for numeric oracle and web UI tests")
        endif()
        message(STATUS "Node.js not found: optional embedded web UI tests are skipped")
    endif()
endif()
