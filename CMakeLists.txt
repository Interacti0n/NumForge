cmake_minimum_required(VERSION 3.20)

project(MyScientificCalc
    VERSION 0.1.0
    DESCRIPTION "Scientific calculator and mathematics library written in C"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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