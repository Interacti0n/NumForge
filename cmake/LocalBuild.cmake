cmake_minimum_required(VERSION 3.21)

# Invoke from any directory with:
#   cmake -P path/to/NumForge/cmake/LocalBuild.cmake
# For a library/app build without tests:
#   cmake -DNUMFORGE_LOCAL_TESTS=OFF -P path/to/NumForge/cmake/LocalBuild.cmake
# Build and open both Windows apps:
#   cmake -DNUMFORGE_LOCAL_TESTS=OFF -DNUMFORGE_LOCAL_RUN=both -P cmake/LocalBuild.cmake

get_filename_component(numforge_local_source "${CMAKE_CURRENT_LIST_DIR}/.." REALPATH)
if(NOT DEFINED NUMFORGE_LOCAL_TESTS)
    set(NUMFORGE_LOCAL_TESTS ON)
endif()

if(NOT DEFINED NUMFORGE_LOCAL_RUN)
    set(NUMFORGE_LOCAL_RUN none)
endif()
if(NOT NUMFORGE_LOCAL_RUN MATCHES "^(none|cli|web|both)$")
    message(FATAL_ERROR "NUMFORGE_LOCAL_RUN must be none, cli, web or both.")
endif()
if(NOT NUMFORGE_LOCAL_RUN STREQUAL "none" AND NOT CMAKE_HOST_WIN32)
    message(FATAL_ERROR "Automatic app launching currently supports Windows only.")
endif()
if(NOT DEFINED NUMFORGE_LOCAL_PORT)
    set(NUMFORGE_LOCAL_PORT 8765)
endif()
if(NOT NUMFORGE_LOCAL_PORT MATCHES "^[0-9]+$" OR
   NUMFORGE_LOCAL_PORT LESS 1 OR NUMFORGE_LOCAL_PORT GREATER 65535)
    message(FATAL_ERROR "NUMFORGE_LOCAL_PORT must be an integer from 1 to 65535.")
endif()

if(NUMFORGE_LOCAL_TESTS)
    set(numforge_local_preset dev)
else()
    set(numforge_local_preset apps)
endif()
set(numforge_local_build "${numforge_local_source}/build/${numforge_local_preset}")
message(STATUS "NumForge local build: ${numforge_local_build}")

execute_process(COMMAND "${CMAKE_COMMAND}" --preset "${numforge_local_preset}"
    WORKING_DIRECTORY "${numforge_local_source}" RESULT_VARIABLE numforge_local_result)
if(NOT numforge_local_result EQUAL 0)
    message(FATAL_ERROR "CMake configuration failed (${numforge_local_result}).")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build --preset "${numforge_local_preset}"
    WORKING_DIRECTORY "${numforge_local_source}" RESULT_VARIABLE numforge_local_result)
if(NOT numforge_local_result EQUAL 0)
    message(FATAL_ERROR "Build failed (${numforge_local_result}).")
endif()

if(CMAKE_HOST_WIN32 AND EXISTS "${numforge_local_build}/Release/numforge_web.exe")
    message(STATUS "Web executable: ${numforge_local_build}/Release/numforge_web.exe")
elseif(CMAKE_HOST_WIN32 AND EXISTS "${numforge_local_build}/numforge_web.exe")
    message(STATUS "Web executable: ${numforge_local_build}/numforge_web.exe")
elseif(EXISTS "${numforge_local_build}/numforge_web")
    message(STATUS "Web executable: ${numforge_local_build}/numforge_web")
endif()

if(NUMFORGE_LOCAL_TESTS)
    execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --preset dev
        WORKING_DIRECTORY "${numforge_local_source}" RESULT_VARIABLE numforge_local_result)
    if(NOT numforge_local_result EQUAL 0)
        message(FATAL_ERROR "Tests failed (${numforge_local_result}).")
    endif()
endif()

if(NOT NUMFORGE_LOCAL_RUN STREQUAL "none")
    find_program(numforge_local_powershell NAMES powershell pwsh REQUIRED)
    execute_process(COMMAND "${numforge_local_powershell}" -NoProfile
        -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_LIST_DIR}/RunLocalApps.ps1"
        -BuildDirectory "${numforge_local_build}" -Mode "${NUMFORGE_LOCAL_RUN}"
        -Port "${NUMFORGE_LOCAL_PORT}" RESULT_VARIABLE numforge_local_result)
    if(NOT numforge_local_result EQUAL 0)
        message(FATAL_ERROR "App launch failed (${numforge_local_result}).")
    endif()
endif()
