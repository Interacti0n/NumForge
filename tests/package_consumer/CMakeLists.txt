cmake_minimum_required(VERSION 3.20)

project(NumForgePackageConsumer LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(NumForge 1.0 CONFIG REQUIRED)

add_executable(numforge_package_consumer
    main.c
)

target_link_libraries(numforge_package_consumer
    PRIVATE
        NumForge::numforge
)

enable_testing()
add_test(
    NAME numforge_package_consumer
    COMMAND numforge_package_consumer
)
