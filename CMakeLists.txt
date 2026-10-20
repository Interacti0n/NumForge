cmake_minimum_required(VERSION 3.20)

project(NumForge
    VERSION 2.4.0
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
option(NUMFORGE_BUILD_ENGINE "Build and install the standalone expression engine" ON)
option(NUMFORGE_BUILD_PYTHON "Build the optional CPython extension" OFF)

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
    # Repository-qualified private includes never enter installed interfaces.
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}")
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
                "/FI${PROJECT_SOURCE_DIR}/src/runtime/instrumentation/benchmark_redirect.h")
        else()
            target_compile_options(${target} PRIVATE -include
                "${PROJECT_SOURCE_DIR}/src/runtime/instrumentation/benchmark_redirect.h")
        endif()
    endif()
    if(NUMFORGE_ENABLE_COVERAGE)
        target_compile_options(${target} PRIVATE --coverage -fprofile-abs-path -Og -g)
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

add_library(numforge STATIC)
add_subdirectory(src/runtime)
add_subdirectory(src/core)
add_subdirectory(src/modules)

# Public expression engine; application state and HTTP remain private.
if(NOT NUMFORGE_BUILD_ENGINE AND
   (NUMFORGE_BUILD_APPS OR BUILD_TESTING OR NUMFORGE_BUILD_FUZZERS OR NUMFORGE_BUILD_BENCHMARKS OR NUMFORGE_BUILD_PYTHON))
    message(FATAL_ERROR "Applications, tests, fuzzers, benchmarks and Python require NUMFORGE_BUILD_ENGINE=ON")
endif()
if(NUMFORGE_BUILD_ENGINE)
    add_library(numforge_engine STATIC)
    add_subdirectory(src/engine)
    add_library(NumForge::engine ALIAS numforge_engine)
    add_library(numforge_calculator ALIAS numforge_engine)
    set_target_properties(numforge_engine PROPERTIES EXPORT_NAME engine)
    target_link_libraries(numforge_engine PUBLIC numforge)
    if(WIN32)
        target_link_libraries(numforge_engine PRIVATE bcrypt)
    endif()
    if(NOT MSVC)
        target_link_libraries(numforge_engine PUBLIC m)
    endif()
    numforge_enable_diagnostics(numforge_engine)
endif()

if(NUMFORGE_BUILD_APPS OR BUILD_TESTING OR NUMFORGE_BUILD_FUZZERS OR NUMFORGE_BUILD_BENCHMARKS)
    add_library(numforge_application STATIC)
    add_subdirectory(src/application)
    target_link_libraries(numforge_application PUBLIC numforge_calculator)
    if(NOT MSVC)
        target_link_libraries(numforge_application PUBLIC m)
    endif()
    numforge_enable_diagnostics(numforge_application)

    add_library(numforge_client STATIC)
    add_subdirectory(apps/web/http)
    target_link_libraries(numforge_client PUBLIC numforge_application)
    numforge_enable_diagnostics(numforge_client)
endif()

add_library(NumForge::numforge ALIAS numforge)

target_include_directories(numforge
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

numforge_enable_diagnostics(numforge)

if(NUMFORGE_BUILD_PYTHON)
    if(NUMFORGE_BUILD_BENCHMARKS OR NUMFORGE_ENABLE_COVERAGE OR NUMFORGE_BUILD_FUZZERS)
        message(FATAL_ERROR "Build Python separately from benchmark/coverage/fuzzer instrumentation")
    endif()
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter Development.Module)
    set_target_properties(numforge numforge_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
    Python3_add_library(numforge_python MODULE WITH_SOABI)
    add_subdirectory(bindings/python)
    set_target_properties(numforge_python PROPERTIES OUTPUT_NAME _native)
    target_link_libraries(numforge_python PRIVATE numforge_engine)
    target_compile_definitions(numforge_python PRIVATE NUMFORGE_CORE_VERSION="${PROJECT_VERSION}")
    numforge_enable_diagnostics(numforge_python)
    if(DEFINED NUMFORGE_PYTHON_OUTPUT_DIRECTORY)
        set_target_properties(numforge_python PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${NUMFORGE_PYTHON_OUTPUT_DIRECTORY}")
        foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            set_target_properties(numforge_python PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY_${config} "${NUMFORGE_PYTHON_OUTPUT_DIRECTORY}")
        endforeach()
    endif()
endif()

if(BUILD_TESTING)
    target_compile_definitions(numforge PRIVATE
        NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
    )
endif()

# ============================================================
# Calculator application
# ============================================================

if(NUMFORGE_BUILD_APPS)
add_executable(calculator)
add_subdirectory(apps/cli)

target_link_libraries(calculator
    PRIVATE
        numforge_application
)
numforge_enable_diagnostics(calculator)

# ============================================================
# Local web application
# ============================================================

add_subdirectory(apps/web/assets)
set(NUMFORGE_WEB_GENERATED_DIR "${PROJECT_BINARY_DIR}/generated/web")
set(NUMFORGE_WEB_GENERATED_HEADER "${NUMFORGE_WEB_GENERATED_DIR}/web_page.h")
add_custom_command(
    OUTPUT "${NUMFORGE_WEB_GENERATED_HEADER}"
    COMMAND "${CMAKE_COMMAND}"
        "-DNUMFORGE_WEB_SOURCE_DIR=${PROJECT_SOURCE_DIR}/apps/web/assets"
        "-DNUMFORGE_WEB_OUTPUT=${NUMFORGE_WEB_GENERATED_HEADER}"
        "-DNUMFORGE_WEB_VERSION=${PROJECT_VERSION}"
        -P "${PROJECT_SOURCE_DIR}/cmake/EmbedWeb.cmake"
    DEPENDS "${PROJECT_SOURCE_DIR}/cmake/EmbedWeb.cmake"
        ${NUMFORGE_WEB_ASSETS}
    VERBATIM
)

add_executable(numforge_web "${NUMFORGE_WEB_GENERATED_HEADER}")
add_subdirectory(apps/web/server)

target_include_directories(numforge_web
    PRIVATE
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
if(NUMFORGE_BUILD_ENGINE)
    install(TARGETS numforge_engine EXPORT NumForgeTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

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
    add_executable(numerical_phase_benchmark benchmarks/numerical_phase_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(numerical_phase_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(numerical_phase_benchmark)
    if(BUILD_TESTING)
        add_test(NAME numerical_phase_benchmark_smoke COMMAND numerical_phase_benchmark)
        set_tests_properties(numerical_phase_benchmark_smoke PROPERTIES TIMEOUT 120)
    endif()
    add_executable(numerical_curve_benchmark benchmarks/numerical_curve_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(numerical_curve_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(numerical_curve_benchmark)
    if(BUILD_TESTING)
        add_test(NAME numerical_curve_benchmark_smoke COMMAND numerical_curve_benchmark)
        set_tests_properties(numerical_curve_benchmark_smoke PROPERTIES TIMEOUT 120)
    endif()
    add_executable(numerical_refinement_benchmark benchmarks/numerical_refinement_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(numerical_refinement_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(numerical_refinement_benchmark)
    if(BUILD_TESTING)
        add_test(NAME numerical_refinement_benchmark_smoke COMMAND numerical_refinement_benchmark)
        set_tests_properties(numerical_refinement_benchmark_smoke PROPERTIES TIMEOUT 120)
    endif()
    add_executable(numerical_grid_benchmark benchmarks/numerical_grid_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(numerical_grid_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(numerical_grid_benchmark)
    if(BUILD_TESTING)
        add_test(NAME numerical_grid_benchmark_smoke COMMAND numerical_grid_benchmark)
        set_tests_properties(numerical_grid_benchmark_smoke PROPERTIES TIMEOUT 120)
    endif()
    add_executable(numerical_scan_benchmark benchmarks/numerical_scan_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(numerical_scan_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(numerical_scan_benchmark)
    if(BUILD_TESTING)
        add_test(NAME numerical_scan_benchmark_smoke COMMAND numerical_scan_benchmark)
        set_tests_properties(numerical_scan_benchmark_smoke PROPERTIES TIMEOUT 120)
    endif()
    add_executable(numerical_benchmark benchmarks/numerical_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(numerical_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(numerical_benchmark)
    if(BUILD_TESTING)
        add_test(NAME numerical_benchmark_smoke COMMAND numerical_benchmark)
        set_tests_properties(numerical_benchmark_smoke PROPERTIES TIMEOUT 120)
    endif()
    add_executable(statistics_benchmark benchmarks/statistics_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(statistics_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(statistics_benchmark)
    if(BUILD_TESTING)
        add_test(NAME statistics_benchmark_smoke COMMAND statistics_benchmark)
        set_tests_properties(statistics_benchmark_smoke PROPERTIES TIMEOUT 120)
    endif()
    add_executable(matrix_benchmark benchmarks/matrix_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(matrix_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(matrix_benchmark)
    if(BUILD_TESTING)
        add_test(NAME matrix_benchmark_smoke COMMAND matrix_benchmark)
        set_tests_properties(matrix_benchmark_smoke PROPERTIES TIMEOUT 120)
    endif()
    add_executable(polynomial_benchmark benchmarks/polynomial_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(polynomial_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(polynomial_benchmark)
    target_sources(numforge PRIVATE src/runtime/instrumentation/benchmark_profile.c src/runtime/instrumentation/decimal_profile.c)
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
    add_executable(graph_benchmark benchmarks/graph_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(graph_benchmark PRIVATE numforge_client)
    if(UNIX)
        target_link_libraries(graph_benchmark PRIVATE m)
    endif()
    numforge_enable_diagnostics(graph_benchmark)
    add_executable(graph_fast_benchmark benchmarks/graph_fast_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(graph_fast_benchmark PRIVATE numforge_application)
    numforge_enable_diagnostics(graph_fast_benchmark)
    add_executable(graph_verify_benchmark benchmarks/graph_verify_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(graph_verify_benchmark PRIVATE numforge_application)
    numforge_enable_diagnostics(graph_verify_benchmark)
    add_executable(graph_brent_benchmark benchmarks/graph_brent_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(graph_brent_benchmark PRIVATE numforge_application)
    numforge_enable_diagnostics(graph_brent_benchmark)
    add_executable(numeric_root_benchmark benchmarks/numeric_root_benchmark.c benchmarks/benchmark_clock.c)
    target_compile_definitions(numeric_root_benchmark PRIVATE NUMFORGE_ENABLE_ALLOC_STATS)
    target_link_libraries(numeric_root_benchmark PRIVATE numforge_application)
    numforge_enable_diagnostics(numeric_root_benchmark)
    add_executable(numeric_root_scan_benchmark benchmarks/numeric_root_scan_benchmark.c benchmarks/benchmark_clock.c)
    target_compile_definitions(numeric_root_scan_benchmark PRIVATE NUMFORGE_ENABLE_ALLOC_STATS)
    target_link_libraries(numeric_root_scan_benchmark PRIVATE numforge_application)
    numforge_enable_diagnostics(numeric_root_scan_benchmark)
    if(BUILD_TESTING)
        add_test(NAME numeric_root_scan_benchmark_smoke COMMAND numeric_root_scan_benchmark --quick)
        add_test(NAME numeric_root_benchmark_smoke COMMAND numeric_root_benchmark --quick)
        add_test(NAME numeric_root_domain_regression COMMAND numeric_root_benchmark --adversarial)
        add_test(NAME numeric_root_brent_smoke COMMAND numeric_root_benchmark --compare --quick)
        add_test(NAME numeric_root_brent_domain_regression COMMAND numeric_root_benchmark --compare --adversarial)
        add_test(NAME numeric_root_hybrid_smoke COMMAND numeric_root_benchmark --hybrid --quick)
        add_test(NAME numeric_root_hybrid_domain_regression COMMAND numeric_root_benchmark --hybrid --adversarial)
        set_tests_properties(numeric_root_brent_smoke numeric_root_brent_domain_regression PROPERTIES TIMEOUT 120)
        set_tests_properties(numeric_root_hybrid_smoke numeric_root_hybrid_domain_regression PROPERTIES TIMEOUT 120)
        set_tests_properties(numeric_root_benchmark_smoke PROPERTIES TIMEOUT 120)
    endif()
    add_executable(calculator_benchmark benchmarks/calculator_benchmark.c benchmarks/benchmark_clock.c)
    target_compile_definitions(calculator_benchmark PRIVATE NUMFORGE_ENABLE_ALLOC_STATS)
    target_link_libraries(calculator_benchmark PRIVATE numforge_client)
    numforge_enable_diagnostics(calculator_benchmark)
    add_executable(calculator_limit_benchmark benchmarks/calculator_limit_benchmark.c benchmarks/benchmark_clock.c)
    target_compile_definitions(calculator_limit_benchmark PRIVATE NUMFORGE_ENABLE_ALLOC_STATS)
    target_link_libraries(calculator_limit_benchmark PRIVATE numforge_client)
    numforge_enable_diagnostics(calculator_limit_benchmark)
    add_executable(bigint_materialization_benchmark benchmarks/bigint_materialization_benchmark.c benchmarks/benchmark_clock.c)
    target_compile_definitions(bigint_materialization_benchmark PRIVATE NUMFORGE_ENABLE_ALLOC_STATS)
    target_link_libraries(bigint_materialization_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(bigint_materialization_benchmark)
    if(BUILD_TESTING)
        add_test(NAME bigint_materialization_benchmark_smoke COMMAND bigint_materialization_benchmark 20)
    endif()
    add_executable(bigint_multiply_benchmark benchmarks/bigint_multiply_benchmark.c benchmarks/benchmark_clock.c)
    target_compile_definitions(bigint_multiply_benchmark PRIVATE NUMFORGE_ENABLE_ALLOC_STATS)
    target_link_libraries(bigint_multiply_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(bigint_multiply_benchmark)
    add_executable(bigint_arithmetic_benchmark benchmarks/bigint_arithmetic_benchmark.c benchmarks/benchmark_clock.c benchmarks/karatsuba_experiment.c)
    target_link_libraries(bigint_arithmetic_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(bigint_arithmetic_benchmark)
    add_executable(decimal_math_benchmark benchmarks/decimal_math_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(decimal_math_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(decimal_math_benchmark)
    add_executable(logarithm_agm_benchmark benchmarks/logarithm_agm_benchmark.c benchmarks/benchmark_clock.c)
    target_link_libraries(logarithm_agm_benchmark PRIVATE numforge)
    numforge_enable_diagnostics(logarithm_agm_benchmark)
    if(BUILD_TESTING)
        target_compile_definitions(logarithm_agm_benchmark PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
        add_test(NAME logarithm_agm_fault_checks COMMAND logarithm_agm_benchmark --fault-checks)
        set_tests_properties(logarithm_agm_fault_checks PROPERTIES TIMEOUT 300)
        foreach(numforge_agm_shard RANGE 0 3)
            add_test(NAME logarithm_agm_interval_fault_checks_${numforge_agm_shard}
                COMMAND logarithm_agm_benchmark --fault-checks-interval ${numforge_agm_shard})
            set_tests_properties(logarithm_agm_interval_fault_checks_${numforge_agm_shard} PROPERTIES TIMEOUT 300)
        endforeach()
        target_compile_definitions(bigint_arithmetic_benchmark PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
        add_test(NAME karatsuba_fault_checks COMMAND bigint_arithmetic_benchmark --karatsuba-check)
        set_tests_properties(karatsuba_fault_checks PROPERTIES TIMEOUT 120)
        add_test(NAME multiplication_fault_checks COMMAND bigint_arithmetic_benchmark --multiply-check)
        set_tests_properties(multiplication_fault_checks PROPERTIES TIMEOUT 120)
        add_test(NAME normalization_fault_checks COMMAND bigint_arithmetic_benchmark --normalization-check)
        set_tests_properties(normalization_fault_checks PROPERTIES TIMEOUT 120)
        find_program(NUMFORGE_BIGINT_BENCH_NODE NAMES node nodejs)
        if(NUMFORGE_BIGINT_BENCH_NODE)
            add_test(NAME karatsuba_reference_smoke
                COMMAND ${NUMFORGE_BIGINT_BENCH_NODE} ${PROJECT_SOURCE_DIR}/benchmarks/check_karatsuba.cjs $<TARGET_FILE:bigint_arithmetic_benchmark>)
            set_tests_properties(karatsuba_reference_smoke PROPERTIES TIMEOUT 180)
            add_test(NAME logarithm_agm_benchmark_smoke
                COMMAND ${NUMFORGE_BIGINT_BENCH_NODE} ${PROJECT_SOURCE_DIR}/benchmarks/check_logarithm_agm_benchmark.cjs $<TARGET_FILE:logarithm_agm_benchmark>)
            set_tests_properties(logarithm_agm_benchmark_smoke PROPERTIES TIMEOUT 120)
            add_test(NAME bigint_arithmetic_benchmark_smoke
                COMMAND ${NUMFORGE_BIGINT_BENCH_NODE} ${PROJECT_SOURCE_DIR}/benchmarks/check_bigint_benchmark.js $<TARGET_FILE:bigint_arithmetic_benchmark>)
            add_test(NAME decimal_math_benchmark_smoke
                COMMAND ${NUMFORGE_BIGINT_BENCH_NODE} ${PROJECT_SOURCE_DIR}/benchmarks/check_decimal_math_benchmark.js $<TARGET_FILE:decimal_math_benchmark>)
            set_tests_properties(decimal_math_benchmark_smoke PROPERTIES TIMEOUT 120)
            add_test(NAME performance_audit_smoke
                COMMAND ${NUMFORGE_BIGINT_BENCH_NODE} ${PROJECT_SOURCE_DIR}/benchmarks/check_performance_audit.js
                    $<TARGET_FILE:decimal_math_benchmark> $<TARGET_FILE:calculator_limit_benchmark>)
            set_tests_properties(performance_audit_smoke PROPERTIES TIMEOUT 30)
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
        target_link_libraries(fuzz_${kind} PRIVATE numforge_client)
        numforge_enable_diagnostics(fuzz_${kind})
        target_link_options(fuzz_${kind} PRIVATE -fsanitize=fuzzer)
    endforeach()
endif()

if(BUILD_TESTING)
    include(FetchContent)

    foreach(kind IN ITEMS parser numbers formatter http)
        add_executable(fuzz_${kind}_smoke tests/fuzz/fuzz_${kind}.c tests/fuzz/replay.c)
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

    # Keep registration consistent while each suite retains its own contracts.
    function(numforge_add_unit_test target library source)
        add_executable(${target} ${source})
        target_link_libraries(${target} PRIVATE ${library} unity)
        numforge_enable_diagnostics(${target})
        add_test(NAME ${target} COMMAND ${target} ${ARGN})
    endfunction()

    # Unity is a test-only dependency. Excluding its source directory keeps
    # its own install rules out of NumForge installation packages.
    set_property(DIRECTORY "${unity_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)

    numforge_add_unit_test(bigint_tests numforge tests/test_bigint.c)

    numforge_add_unit_test(public_api_tests numforge tests/test_public_api.c)

    numforge_add_unit_test(engine_tests numforge_engine tests/test_engine.c)
    target_compile_definitions(engine_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    target_compile_definitions(engine_tests PRIVATE
        NUMFORGE_ENGINE_REFERENCE_FILE="${PROJECT_SOURCE_DIR}/benchmarks/references/decimal_math_references.tsv")

    foreach(suite engine_parameters polynomial matrix linear_system statistics numerical numerical_scan numerical_grid numerical_refinement numerical_curve numerical_phase)
        add_executable(${suite}_tests tests/test_${suite}.c)
        target_compile_definitions(${suite}_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
        numforge_enable_diagnostics(${suite}_tests)
        add_test(NAME ${suite}_tests COMMAND ${suite}_tests)
    endforeach()
    target_link_libraries(engine_parameters_tests PRIVATE numforge_engine unity)
    target_link_libraries(polynomial_tests PRIVATE numforge unity)
    target_link_libraries(matrix_tests PRIVATE numforge unity)
    target_link_libraries(linear_system_tests PRIVATE numforge unity)
    target_link_libraries(statistics_tests PRIVATE numforge unity)
    target_link_libraries(numerical_tests PRIVATE numforge unity)
    target_link_libraries(numerical_scan_tests PRIVATE numforge unity)
    target_link_libraries(numerical_grid_tests PRIVATE numforge unity)
    target_link_libraries(numerical_refinement_tests PRIVATE numforge unity)
    target_link_libraries(numerical_curve_tests PRIVATE numforge unity)
    target_link_libraries(numerical_phase_tests PRIVATE numforge unity)
    set_tests_properties(numerical_scan_tests PROPERTIES TIMEOUT 180)
    set_tests_properties(numerical_tests PROPERTIES TIMEOUT 180)

    numforge_add_unit_test(bigint_property_tests numforge tests/test_bigint_properties.c)

    numforge_add_unit_test(bigdecimal_tests numforge tests/test_bigdecimal.c)

    numforge_add_unit_test(bigrational_tests numforge tests/test_bigrational.c)

    numforge_add_unit_test(bigcomplex_tests numforge tests/test_bigcomplex.c --numeric-only)
    target_compile_definitions(bigcomplex_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    foreach(shard RANGE 0 7)
        add_test(NAME bigcomplex_allocation_${shard} COMMAND bigcomplex_tests --allocation-shard ${shard} 8)
        set_tests_properties(bigcomplex_allocation_${shard} PROPERTIES LABELS allocation TIMEOUT 1500)
    endforeach()
    add_executable(complex_oracle_driver tests/complex_oracle_driver.c benchmarks/benchmark_clock.c)
    target_link_libraries(complex_oracle_driver PRIVATE numforge)
    numforge_enable_diagnostics(complex_oracle_driver)
    find_program(NUMFORGE_COMPLEX_NODE NAMES node nodejs REQUIRED)
    foreach(precision IN ITEMS 12 34 100 250)
        add_test(NAME complex_oracle_${precision} COMMAND "${NUMFORGE_COMPLEX_NODE}"
            "${PROJECT_SOURCE_DIR}/tests/test_complex_oracle.js" $<TARGET_FILE:complex_oracle_driver> --precision=${precision})
        set_tests_properties(complex_oracle_${precision} PROPERTIES TIMEOUT 150)
    endforeach()
    if(NUMFORGE_BUILD_BENCHMARKS)
        add_test(NAME complex_memory_smoke COMMAND "${NUMFORGE_COMPLEX_NODE}"
            "${PROJECT_SOURCE_DIR}/tests/test_complex_oracle.js" $<TARGET_FILE:complex_oracle_driver> --memory --quick)
        set_tests_properties(complex_memory_smoke PROPERTIES TIMEOUT 150)
    endif()

    numforge_add_unit_test(units_tests numforge tests/test_units.c)
    target_compile_definitions(units_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)

    numforge_add_unit_test(bigdecimal_property_tests numforge tests/test_bigdecimal_properties.c)

    numforge_add_unit_test(calculator_tests numforge_client tests/test_calculator.c)

    foreach(suite IN ITEMS calculator_contract parser_fuzz http_request function_calls roots transcendental trigonometric hyperbolic constants web_cache)
        add_executable(${suite}_tests tests/test_${suite}.c)
        target_link_libraries(${suite}_tests PRIVATE numforge_client unity)
        numforge_enable_diagnostics(${suite}_tests)
        add_test(NAME ${suite}_tests COMMAND ${suite}_tests)
        set_tests_properties(${suite}_tests PROPERTIES TIMEOUT 60)
    endforeach()

    numforge_add_unit_test(unit_web_tests numforge_client tests/test_unit_web.c)
    target_compile_definitions(unit_web_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)

    numforge_add_unit_test(web_api_tests numforge_client tests/test_web_api.c)

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

    numforge_add_unit_test(allocation_failure_tests numforge_client tests/test_allocation_failures.c)

    numforge_add_unit_test(session_tests numforge_client tests/test_session.c)
    numforge_add_unit_test(calculator_complex_tests numforge_client tests/test_calculator_complex.c --numeric-only)
    target_compile_definitions(calculator_complex_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    foreach(shard RANGE 0 7)
        add_test(NAME calculator_complex_allocation_${shard} COMMAND calculator_complex_tests --allocation-shard ${shard} 8)
        set_tests_properties(calculator_complex_allocation_${shard} PROPERTIES LABELS allocation TIMEOUT 1500)
    endforeach()
    numforge_add_unit_test(application_tests numforge_application tests/test_application.c)
    numforge_add_unit_test(application_expression_tests numforge_application tests/test_application_expression.c)
    target_compile_definitions(application_expression_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    numforge_add_unit_test(equation_tests numforge_client tests/test_equation.c)
    target_compile_definitions(equation_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    numforge_add_unit_test(numeric_root_tests numforge_client tests/test_numeric_root.c)
    target_compile_definitions(numeric_root_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    numforge_add_unit_test(numeric_root_scan_tests numforge_client tests/test_numeric_root_scan.c)
    target_compile_definitions(numeric_root_scan_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    numforge_add_unit_test(graph_tests numforge_client tests/test_graph.c)
    target_compile_definitions(graph_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    target_compile_definitions(session_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
    set_tests_properties(session_tests PROPERTIES TIMEOUT 60)

    target_compile_definitions(allocation_failure_tests PRIVATE
        NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
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
        add_test(NAME architecture_check COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/scripts/check_architecture.cjs")
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
        target_link_libraries(numeric_oracle_driver PRIVATE numforge_client)
        numforge_enable_diagnostics(numeric_oracle_driver)
        add_test(NAME numeric_oracle_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/test_numeric_oracle.js" $<TARGET_FILE:numeric_oracle_driver>)
        set_tests_properties(numeric_oracle_tests PROPERTIES TIMEOUT 120)
        add_test(NAME division_oracle_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/test_division_oracle.js" $<TARGET_FILE:numeric_oracle_driver>)
        set_tests_properties(division_oracle_tests PROPERTIES TIMEOUT 120)
        add_test(NAME web_ui_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_web_ui.js")
        set_tests_properties(web_ui_tests PROPERTIES TIMEOUT 30)
    else()
        if(NUMFORGE_REQUIRE_NODE_TESTS)
            message(FATAL_ERROR "Node.js is required for numeric oracle and web UI tests")
        endif()
        message(STATUS "Node.js not found: optional embedded web UI tests are skipped")
    endif()
endif()
