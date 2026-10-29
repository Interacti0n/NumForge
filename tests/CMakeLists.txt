include(FetchContent)

foreach(kind IN ITEMS parser numbers formatter http)
    add_executable(fuzz_${kind}_smoke fuzz/fuzz_${kind}.c fuzz/replay.c)
    target_link_libraries(fuzz_${kind}_smoke PRIVATE numforge_client)
    numforge_enable_diagnostics(fuzz_${kind}_smoke)
    add_test(NAME fuzz_${kind}_smoke COMMAND fuzz_${kind}_smoke)
    set_tests_properties(fuzz_${kind}_smoke PROPERTIES TIMEOUT 60)
endforeach()

FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    # Exact v2.6.1 commit for reproducible test dependency resolution.
    GIT_TAG cbcd08fa7de711053a3deec6339ee89cad5d2697
)

FetchContent_MakeAvailable(unity)

# NumForge tests exercise int64_t calculator settings and uint64_t-backed
# counters even when the test executable itself is built for 32 bits.
target_compile_definitions(unity PUBLIC UNITY_SUPPORT_64)

# Keep registration consistent while each suite retains its own contracts.
function(numforge_add_unit_test target library source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ${library} unity)
    numforge_enable_diagnostics(${target})
    add_test(NAME ${target} COMMAND ${target} ${ARGN})
endfunction()

# Unity is a test-only dependency. Excluding its source directory keeps
# its own install rules out of NumForge installation packages.
set_property(DIRECTORY "${unity_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)

numforge_add_unit_test(bigint_tests numforge core/test_bigint.c)

numforge_add_unit_test(public_api_tests numforge core/test_public_api.c)

numforge_add_unit_test(engine_tests numforge_engine engine/test_engine.c)
target_compile_definitions(engine_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
target_compile_definitions(engine_tests PRIVATE
    NUMFORGE_ENGINE_REFERENCE_FILE="${PROJECT_SOURCE_DIR}/benchmarks/references/decimal_math_references.tsv")

numforge_add_unit_test(engine_parameters_tests numforge_engine engine/test_engine_parameters.c)
target_compile_definitions(engine_parameters_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
foreach(suite IN ITEMS polynomial matrix linear_system statistics regression numerical numerical_scan numerical_grid numerical_refinement numerical_curve numerical_phase integral)
    numforge_add_unit_test(${suite}_tests numforge modules/test_${suite}.c)
    target_compile_definitions(${suite}_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
endforeach()
set_tests_properties(numerical_scan_tests PROPERTIES TIMEOUT 180)
set_tests_properties(numerical_tests PROPERTIES TIMEOUT 180)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    # The unchanged 1000-digit root check is much slower with 32-bit wide arithmetic.
    set_tests_properties(numerical_tests PROPERTIES TIMEOUT 360)
endif()

numforge_add_unit_test(bigint_property_tests numforge core/test_bigint_properties.c)

numforge_add_unit_test(bigdecimal_tests numforge core/test_bigdecimal.c)

numforge_add_unit_test(bigrational_tests numforge core/test_bigrational.c)

numforge_add_unit_test(bigcomplex_tests numforge core/test_bigcomplex.c --numeric-only)
target_compile_definitions(bigcomplex_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
foreach(shard RANGE 0 7)
    add_test(NAME bigcomplex_allocation_${shard} COMMAND bigcomplex_tests --allocation-shard ${shard} 8)
    set_tests_properties(bigcomplex_allocation_${shard} PROPERTIES LABELS allocation TIMEOUT 1500)
endforeach()
add_executable(complex_oracle_driver oracles/complex_oracle_driver.c ${PROJECT_SOURCE_DIR}/benchmarks/common/benchmark_clock.c)
target_link_libraries(complex_oracle_driver PRIVATE numforge)
numforge_enable_diagnostics(complex_oracle_driver)
find_program(NUMFORGE_COMPLEX_NODE NAMES node nodejs REQUIRED)
foreach(precision IN ITEMS 12 34 100 250)
    add_test(NAME complex_oracle_${precision} COMMAND "${NUMFORGE_COMPLEX_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/oracles/test_complex_oracle.js" $<TARGET_FILE:complex_oracle_driver> --precision=${precision})
    set_tests_properties(complex_oracle_${precision} PROPERTIES TIMEOUT 150)
endforeach()
if(NUMFORGE_BUILD_BENCHMARKS)
    add_test(NAME complex_memory_smoke COMMAND "${NUMFORGE_COMPLEX_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/oracles/test_complex_oracle.js" $<TARGET_FILE:complex_oracle_driver> --memory --quick)
    set_tests_properties(complex_memory_smoke PROPERTIES TIMEOUT 150)
endif()

numforge_add_unit_test(units_tests numforge modules/test_units.c)
target_compile_definitions(units_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)

numforge_add_unit_test(bigdecimal_property_tests numforge core/test_bigdecimal_properties.c)

numforge_add_unit_test(calculator_tests numforge_client engine/test_calculator.c)

foreach(source IN ITEMS engine/test_calculator_contract.c engine/test_parser_fuzz.c http/test_http_request.c
        engine/test_function_calls.c engine/test_roots.c core/test_transcendental.c
        core/test_trigonometric.c core/test_hyperbolic.c core/test_constants.c application/test_web_cache.c)
    get_filename_component(suite "${source}" NAME_WE)
    string(REGEX REPLACE "^test_" "" suite "${suite}")
    numforge_add_unit_test(${suite}_tests numforge_client "${source}")
    set_tests_properties(${suite}_tests PROPERTIES TIMEOUT 60)
endforeach()

numforge_add_unit_test(unit_web_tests numforge_client http/test_unit_web.c)
target_compile_definitions(unit_web_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
numforge_add_unit_test(currency_web_tests numforge_client http/test_currency_web.c)
target_compile_definitions(currency_web_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)

numforge_add_unit_test(web_api_tests numforge_client http/test_web_api.c)
numforge_add_unit_test(matrix_adapter_tests numforge_client application/test_matrix_adapter.c)
target_compile_definitions(matrix_adapter_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)

if(NUMFORGE_BUILD_APPS)
numforge_add_unit_test(web_workers_tests numforge_web_backend http/test_web_workers.c)
set_tests_properties(web_workers_tests PROPERTIES TIMEOUT 60)
add_executable(web_server_smoke_tests
    http/test_web_server.c
)

if(WIN32)
    target_link_libraries(web_server_smoke_tests PRIVATE ws2_32)
endif()

add_dependencies(web_server_smoke_tests numforge_web)
numforge_enable_diagnostics(web_server_smoke_tests)

add_test(
    NAME web_server_smoke_tests
    COMMAND web_server_smoke_tests $<TARGET_FILE:numforge_web>
)

set_tests_properties(web_server_smoke_tests PROPERTIES TIMEOUT 60)
endif()

numforge_add_unit_test(allocation_failure_tests numforge_client failures/test_allocation_failures.c)

numforge_add_unit_test(session_tests numforge_client application/test_session.c)
numforge_add_unit_test(calculator_complex_tests numforge_client engine/test_calculator_complex.c --numeric-only)
target_compile_definitions(calculator_complex_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
foreach(shard RANGE 0 7)
    add_test(NAME calculator_complex_allocation_${shard} COMMAND calculator_complex_tests --allocation-shard ${shard} 8)
    set_tests_properties(calculator_complex_allocation_${shard} PROPERTIES LABELS allocation TIMEOUT 1500)
endforeach()
numforge_add_unit_test(application_tests numforge_application application/test_application.c)
numforge_add_unit_test(client_scheduling_tests numforge_client application/test_client_scheduling.c)
numforge_add_unit_test(client_policy_tests numforge_client application/test_client_policy.c)
if(NOT WIN32)
    target_link_libraries(application_tests PRIVATE Threads::Threads)
    target_link_libraries(client_scheduling_tests PRIVATE Threads::Threads)
endif()
set_tests_properties(application_tests client_scheduling_tests PROPERTIES TIMEOUT 60)
numforge_add_unit_test(application_expression_tests numforge_application application/test_application_expression.c)
target_compile_definitions(application_expression_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
numforge_add_unit_test(equation_tests numforge_client application/test_equation.c)
target_compile_definitions(equation_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
numforge_add_unit_test(numeric_root_tests numforge_client application/test_numeric_root.c)
target_compile_definitions(numeric_root_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
numforge_add_unit_test(numeric_root_scan_tests numforge_client application/test_numeric_root_scan.c)
target_compile_definitions(numeric_root_scan_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
numforge_add_unit_test(graph_tests numforge_client application/test_graph.c)
target_compile_definitions(graph_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
target_compile_definitions(session_tests PRIVATE NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
set_tests_properties(session_tests PROPERTIES TIMEOUT 60)

target_compile_definitions(allocation_failure_tests PRIVATE
    NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
)

set_tests_properties(
    bigint_tests
    bigint_property_tests
    bigdecimal_tests
    bigdecimal_property_tests
    calculator_tests
    web_api_tests
    allocation_failure_tests
    PROPERTIES TIMEOUT 60
)

find_program(NUMFORGE_NODE_EXECUTABLE NAMES node nodejs HINTS "C:/Program Files/nodejs")
if(NUMFORGE_NODE_EXECUTABLE)
    add_test(NAME architecture_check COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/scripts/check_architecture.cjs")
    add_test(NAME architecture_guard_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/test_architecture.cjs")
    add_test(NAME unit_catalog_check COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/scripts/generate_unit_catalog.cjs" --check)
    if(NUMFORGE_BUILD_APPS)
    add_test(NAME cli_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/test_cli.js" $<TARGET_FILE:calculator>)
    set_tests_properties(cli_tests PROPERTIES TIMEOUT 60)
    add_test(NAME web_remote_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/test_web_remote.js" $<TARGET_FILE:numforge_web>)
    set_tests_properties(web_remote_tests PROPERTIES TIMEOUT 60)
    if(WIN32)
        find_program(NUMFORGE_POWERSHELL NAMES powershell REQUIRED)
        add_executable(remote_tunnel_fixture http/remote_tunnel_fixture.c)
        numforge_enable_diagnostics(remote_tunnel_fixture)
        add_test(NAME remote_helper_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/test_remote_helper.cjs" $<TARGET_FILE:numforge_web>
            $<TARGET_FILE:remote_tunnel_fixture> "${NUMFORGE_POWERSHELL}")
        set_tests_properties(remote_helper_tests PROPERTIES TIMEOUT 180)
    endif()
    endif()
    add_executable(numeric_oracle_driver oracles/numeric_oracle_driver.c)
    target_link_libraries(numeric_oracle_driver PRIVATE numforge_client)
    numforge_enable_diagnostics(numeric_oracle_driver)
    add_test(NAME numeric_oracle_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/oracles/test_numeric_oracle.js" $<TARGET_FILE:numeric_oracle_driver>)
    set_tests_properties(numeric_oracle_tests PROPERTIES TIMEOUT 120)
    add_test(NAME division_oracle_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/oracles/test_division_oracle.js" $<TARGET_FILE:numeric_oracle_driver>)
    set_tests_properties(division_oracle_tests PROPERTIES TIMEOUT 120)
    add_test(NAME web_ui_tests COMMAND "${NUMFORGE_NODE_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_web_ui.js")
    set_tests_properties(web_ui_tests PROPERTIES TIMEOUT 30)
else()
    if(NUMFORGE_REQUIRE_NODE_TESTS)
        message(FATAL_ERROR "Node.js is required for numeric oracle and web UI tests")
    endif()
    message(STATUS "Node.js not found: optional embedded web UI tests are skipped")
endif()

# Keep binary locations stable for external consumers and benchmark recorders.
get_property(test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target IN LISTS test_targets)
    get_target_property(kind ${target} TYPE)
    if(kind STREQUAL "EXECUTABLE")
        set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
    endif()
endforeach()
