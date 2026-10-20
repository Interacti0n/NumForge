target_sources(numforge_python PRIVATE
    native.c
    native_support.c
    numerical.c
    polynomial.c
    matrix.c
    statistics.c
)
target_include_directories(numforge_python PRIVATE "${PROJECT_SOURCE_DIR}")
