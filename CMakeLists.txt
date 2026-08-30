cmake_minimum_required(VERSION 3.20)

project(NumForge
    VERSION 0.1.0
    DESCRIPTION "Scientific calculator and mathematics library written in C"
    LANGUAGES C
)

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

function(numforge_enable_diagnostics target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
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

add_library(numforge
    src/bigint/bigint.c
    src/bigdecimal/bigdecimal.c
    src/calculator/calculator.c
    src/calculator/constants.c
    src/calculator/tokenizer.c
    src/calculator/parser.c
    src/calculator/evaluator.c
    src/calculator/formatter.c
    src/web/web_api.c
)

target_include_directories(numforge
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/calculator
        ${PROJECT_SOURCE_DIR}/src/web
)

numforge_enable_diagnostics(numforge)

# ============================================================
# Calculator application
# ============================================================

add_executable(calculator
    src/main.c
)

target_include_directories(calculator
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src/calculator
)

target_link_libraries(calculator
    PRIVATE
        numforge
)
numforge_enable_diagnostics(calculator)

# ============================================================
# Local web application
# ============================================================

add_executable(numforge_web
    src/web/web_server.c
)

target_include_directories(numforge_web
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/calculator
        ${PROJECT_SOURCE_DIR}/src/web
)

target_link_libraries(numforge_web
    PRIVATE
        numforge
)

if(WIN32)
    target_link_libraries(numforge_web PRIVATE ws2_32 shell32)
endif()

numforge_enable_diagnostics(numforge_web)
# ============================================================
# Tests
# ============================================================

include(CTest)

if(BUILD_TESTING)
    include(FetchContent)

    FetchContent_Declare(
        unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG v2.6.1
    )

    FetchContent_MakeAvailable(unity)

    add_executable(bigint_tests
        tests/test_bigint.c
    )

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
        numforge
        unity
    )
    numforge_enable_diagnostics(calculator_tests)

    add_test(
        NAME calculator_tests
        COMMAND calculator_tests
    )

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
            numforge
            unity
    )
    numforge_enable_diagnostics(web_api_tests)

    add_test(
        NAME web_api_tests
        COMMAND web_api_tests
    )
endif()
