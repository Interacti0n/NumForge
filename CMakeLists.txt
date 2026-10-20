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
    FILES README.md CHANGELOG.md LICENSE
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
    add_subdirectory(benchmarks)
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
    add_subdirectory(tests)
endif()
