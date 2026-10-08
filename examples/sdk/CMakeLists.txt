cmake_minimum_required(VERSION 3.20)
project(NumForgeExample LANGUAGES C)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../lib/cmake/NumForge/NumForgeConfig.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
find_package(NumForge 2.1 CONFIG REQUIRED)
add_executable(numforge_example main.c)
target_compile_features(numforge_example PRIVATE c_std_17)
target_link_libraries(numforge_example PRIVATE NumForge::numforge)

enable_testing()
add_test(NAME exact_decimal_example COMMAND numforge_example)
set_tests_properties(exact_decimal_example PROPERTIES PASS_REGULAR_EXPRESSION "0\\.1 \\+ 0\\.2 = 0\\.3")
