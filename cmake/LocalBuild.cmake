cmake_minimum_required(VERSION 3.20)

# Invoke from any directory with:
#   cmake -P path/to/NumForge/cmake/LocalBuild.cmake
# For a library/app build without tests:
#   cmake -DNUMFORGE_LOCAL_TESTS=OFF -P path/to/NumForge/cmake/LocalBuild.cmake
# Build and open both Windows apps:
#   cmake -DNUMFORGE_LOCAL_TESTS=OFF -DNUMFORGE_LOCAL_RUN=both -P cmake/LocalBuild.cmake

get_filename_component(numforge_local_source "${CMAKE_CURRENT_LIST_DIR}/.." REALPATH)
file(TO_CMAKE_PATH "${numforge_local_source}" numforge_local_source)
string(SHA256 numforge_local_source_hash "${numforge_local_source}")
string(SUBSTRING "${numforge_local_source_hash}" 0 12 numforge_local_source_key)

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
    set(numforge_local_kind tests)
else()
    set(numforge_local_kind apps)
endif()

set(numforge_local_build
    "${numforge_local_source}/build/local-${numforge_local_kind}-${numforge_local_source_key}")
set(numforge_local_cache "${numforge_local_build}/CMakeCache.txt")

if(EXISTS "${numforge_local_cache}")
    file(STRINGS "${numforge_local_cache}" numforge_local_home
        REGEX "^CMAKE_HOME_DIRECTORY:INTERNAL=")
    file(STRINGS "${numforge_local_cache}" numforge_local_cache_dir
        REGEX "^CMAKE_CACHEFILE_DIR:INTERNAL=")
    string(REGEX REPLACE "^[^=]*=" "" numforge_local_home "${numforge_local_home}")
    string(REGEX REPLACE "^[^=]*=" "" numforge_local_cache_dir "${numforge_local_cache_dir}")
    file(TO_CMAKE_PATH "${numforge_local_home}" numforge_local_home)
    file(TO_CMAKE_PATH "${numforge_local_cache_dir}" numforge_local_cache_dir)

    if(CMAKE_HOST_WIN32)
        string(TOLOWER "${numforge_local_home}" numforge_local_home)
        string(TOLOWER "${numforge_local_cache_dir}" numforge_local_cache_dir)
        string(TOLOWER "${numforge_local_source}" numforge_local_expected_home)
        string(TOLOWER "${numforge_local_build}" numforge_local_expected_build)
    else()
        set(numforge_local_expected_home "${numforge_local_source}")
        set(numforge_local_expected_build "${numforge_local_build}")
    endif()

    if(NOT numforge_local_home STREQUAL numforge_local_expected_home OR
       NOT numforge_local_cache_dir STREQUAL numforge_local_expected_build)
        message(FATAL_ERROR
            "This build tree belongs to another project location: ${numforge_local_build}. "
            "Keep it for inspection and choose a fresh build directory.")
    endif()
endif()

set(numforge_local_configure
    "${CMAKE_COMMAND}" -S "${numforge_local_source}" -B "${numforge_local_build}"
    "-DBUILD_TESTING=${NUMFORGE_LOCAL_TESTS}"
    "-DNUMFORGE_WARNINGS_AS_ERRORS=ON")

# Visual Studio selects Release at build/test time; single-config generators
# need the build type during configuration.
if(NOT CMAKE_HOST_WIN32 OR
   "$ENV{CMAKE_GENERATOR}" MATCHES "^(Ninja|Unix Makefiles|MinGW Makefiles|NMake Makefiles)$")
    list(APPEND numforge_local_configure "-DCMAKE_BUILD_TYPE=Release")
endif()

# Reuse the already fetched test framework without reusing CMake's old cache.
if(NUMFORGE_LOCAL_TESTS AND EXISTS "${numforge_local_source}/build/_deps/unity-src/CMakeLists.txt")
    list(APPEND numforge_local_configure
        "-DFETCHCONTENT_SOURCE_DIR_UNITY=${numforge_local_source}/build/_deps/unity-src")
endif()

message(STATUS "NumForge local build: ${numforge_local_build}")
execute_process(COMMAND ${numforge_local_configure}
    RESULT_VARIABLE numforge_local_result)
if(NOT numforge_local_result EQUAL 0)
    message(FATAL_ERROR "CMake configuration failed (${numforge_local_result}).")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" --build "${numforge_local_build}"
    --config Release --parallel 1 RESULT_VARIABLE numforge_local_result)
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
    execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${numforge_local_build}"
        -C Release --output-on-failure RESULT_VARIABLE numforge_local_result)
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
