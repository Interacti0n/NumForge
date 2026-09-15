cmake_minimum_required(VERSION 3.20)

project(NumForgePackageConsumer LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(NumForge 1.0 CONFIG REQUIRED)

if(TARGET NumForge::numforge_client)
    message(FATAL_ERROR "Private client target must not be exported with numeric API")
endif()

add_executable(numforge_package_consumer
    main.c
)

target_link_libraries(numforge_package_consumer
    PRIVATE
        NumForge::numforge
)

enable_testing()
option(NUMFORGE_TEST_CPP "Also verify public headers and linkage from C++" OFF)
if(NUMFORGE_TEST_CPP)
    enable_language(CXX)
    add_executable(numforge_cpp_consumer main.cpp)
    target_compile_features(numforge_cpp_consumer PRIVATE cxx_std_11)
    target_link_libraries(numforge_cpp_consumer PRIVATE NumForge::numforge)
    add_test(NAME numforge_cpp_consumer COMMAND numforge_cpp_consumer)
endif()
add_test(
    NAME numforge_package_consumer
    COMMAND numforge_package_consumer
)
