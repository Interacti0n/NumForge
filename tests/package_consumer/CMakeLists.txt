cmake_minimum_required(VERSION 3.20)

project(NumForgePackageConsumer LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(NumForge 2.0 CONFIG REQUIRED)

foreach(private_target numforge_calculator numforge_application numforge_client)
    if(TARGET NumForge::${private_target})
        message(FATAL_ERROR "Private application target must not be exported with numeric API")
    endif()
endforeach()

add_executable(numforge_package_consumer
    main.c
)

target_link_libraries(numforge_package_consumer
    PRIVATE
        NumForge::numforge
)

enable_testing()
option(NUMFORGE_TEST_CPP "Also verify public headers and linkage from C++" OFF)
if(EXISTS "${NumForge_DIR}/../../../include/numforge/matrix.h")
    add_executable(numforge_matrix_consumer matrix.c)
    target_link_libraries(numforge_matrix_consumer PRIVATE NumForge::numforge)
    add_test(NAME numforge_matrix_consumer COMMAND numforge_matrix_consumer)
endif()
if(EXISTS "${NumForge_DIR}/../../../include/numforge/polynomial.h")
    add_executable(numforge_polynomial_consumer polynomial.c)
    target_link_libraries(numforge_polynomial_consumer PRIVATE NumForge::numforge)
    add_test(NAME numforge_polynomial_consumer COMMAND numforge_polynomial_consumer)
endif()
option(NUMFORGE_TEST_ENGINE "Require and verify the public expression engine" ON)
if(NUMFORGE_TEST_ENGINE)
    if(NOT TARGET NumForge::engine)
        message(FATAL_ERROR "Engine consumer requires NumForge::engine; use NUMFORGE_TEST_ENGINE=OFF for numeric-only builds")
    endif()
    add_executable(numforge_engine_consumer engine.c)
    target_link_libraries(numforge_engine_consumer PRIVATE NumForge::engine)
    add_test(NAME numforge_engine_consumer COMMAND numforge_engine_consumer)
endif()
if(NUMFORGE_TEST_CPP)
    enable_language(CXX)
    add_executable(numforge_cpp_consumer main.cpp)
    target_compile_features(numforge_cpp_consumer PRIVATE cxx_std_11)
    target_link_libraries(numforge_cpp_consumer PRIVATE NumForge::numforge)
    add_test(NAME numforge_cpp_consumer COMMAND numforge_cpp_consumer)
    if(TARGET numforge_matrix_consumer)
        add_executable(numforge_matrix_cpp_consumer matrix.cpp)
        target_compile_features(numforge_matrix_cpp_consumer PRIVATE cxx_std_11)
        target_link_libraries(numforge_matrix_cpp_consumer PRIVATE NumForge::numforge)
        add_test(NAME numforge_matrix_cpp_consumer COMMAND numforge_matrix_cpp_consumer)
    endif()
    if(TARGET numforge_polynomial_consumer)
        add_executable(numforge_polynomial_cpp_consumer polynomial.cpp)
        target_compile_features(numforge_polynomial_cpp_consumer PRIVATE cxx_std_11)
        target_link_libraries(numforge_polynomial_cpp_consumer PRIVATE NumForge::numforge)
        add_test(NAME numforge_polynomial_cpp_consumer COMMAND numforge_polynomial_cpp_consumer)
    endif()
    if(NUMFORGE_TEST_ENGINE)
        add_executable(numforge_engine_cpp_consumer engine.cpp)
        target_compile_features(numforge_engine_cpp_consumer PRIVATE cxx_std_11)
        target_link_libraries(numforge_engine_cpp_consumer PRIVATE NumForge::engine)
        add_test(NAME numforge_engine_cpp_consumer COMMAND numforge_engine_cpp_consumer)
    endif()
endif()
add_test(
    NAME numforge_package_consumer
    COMMAND numforge_package_consumer
)
