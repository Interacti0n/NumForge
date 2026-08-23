cmake_minimum_required(VERSION 3.20)

project(MyScientificCalc
    VERSION 0.1.0
    DESCRIPTION "Scientific calculator and mathematics library written in C"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MYSCIENCECALC_WARNINGS_AS_ERRORS
    "Treat warnings in MyScientificCalc sources as errors"
    OFF
)
option(MYSCIENCECALC_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer where supported"
    OFF
)

function(mysciencecalc_enable_diagnostics target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(MYSCIENCECALC_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(MYSCIENCECALC_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    if(MYSCIENCECALC_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

# ============================================================
# MyScientificCalc library
# ============================================================

add_library(mysciencecalc
    src/bigint/bigint.c
)

target_include_directories(mysciencecalc
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

mysciencecalc_enable_diagnostics(mysciencecalc)

# ============================================================
# Calculator application
# ============================================================

add_executable(calculator
    src/main.c
)

target_link_libraries(calculator
    PRIVATE
        mysciencecalc
)
mysciencecalc_enable_diagnostics(calculator)
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
        mysciencecalc
        unity
    )
    mysciencecalc_enable_diagnostics(bigint_tests)

    add_test(
        NAME bigint_tests
        COMMAND bigint_tests
    )

    add_executable(bigint_property_tests
        tests/test_bigint_properties.c
    )

    target_link_libraries(bigint_property_tests
        PRIVATE
        mysciencecalc
        unity
    )
    mysciencecalc_enable_diagnostics(bigint_property_tests)

    add_test(
        NAME bigint_property_tests
        COMMAND bigint_property_tests
    )
endif()
