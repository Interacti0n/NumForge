cmake_minimum_required(VERSION 3.20)

project(MyScientificCalc
    VERSION 0.1.0
    DESCRIPTION "Scientific calculator and mathematics library written in C"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.6.1
)

FetchContent_MakeAvailable(unity)

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
# ============================================================
# Tests
# ============================================================

include(CTest)
enable_testing()

add_executable(bigint_tests
    tests/test_bigint.c
)

target_link_libraries(bigint_tests
    PRIVATE
    mysciencecalc
    unity
)

add_test(
    NAME bigint_tests
    COMMAND bigint_tests
)