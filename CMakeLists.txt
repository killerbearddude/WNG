cmake_minimum_required(VERSION 3.16)

project(WNG VERSION 0.1.0 LANGUAGES CXX)

add_library(wng_core
    src/graph.cpp
    src/validation.cpp
    src/mutation_summary.cpp
    src/serialization.cpp
)

target_include_directories(wng_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(wng_core PUBLIC cxx_std_17)
set_target_properties(wng_core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(wng_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

function(wng_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE wng_core)
    target_compile_features(${name} PRIVATE cxx_std_17)
    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

wng_add_test(test_ids tests/test_ids.cpp)
wng_add_test(test_graph_nodes tests/test_graph_nodes.cpp)
wng_add_test(test_graph_ports tests/test_graph_ports.cpp)
wng_add_test(test_graph_links tests/test_graph_links.cpp)
wng_add_test(test_validation tests/test_validation.cpp)
wng_add_test(test_mutation_summary tests/test_mutation_summary.cpp)
wng_add_test(test_public_header_policy tests/test_public_header_policy.cpp)
wng_add_test(test_serialization_dto tests/test_serialization_dto.cpp)
wng_add_test(test_export_graph tests/test_export_graph.cpp)
wng_add_test(test_import_graph tests/test_import_graph.cpp)
set_tests_properties(test_public_header_policy PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

function(wng_add_header_compile_test name header)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/${name}.cpp")
    file(WRITE "${source}" "#include <${header}>\nint main() { return 0; }\n")

    add_executable(${name} "${source}")
    target_link_libraries(${name} PRIVATE wng_core)
    target_compile_features(${name} PRIVATE cxx_std_17)
    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME ${name} COMMAND ${name})
endfunction()

wng_add_header_compile_test(test_header_ids wng/ids.hpp)
wng_add_header_compile_test(test_header_math wng/math.hpp)
wng_add_header_compile_test(test_header_result wng/result.hpp)
wng_add_header_compile_test(test_header_node wng/node.hpp)
wng_add_header_compile_test(test_header_port wng/port.hpp)
wng_add_header_compile_test(test_header_link wng/link.hpp)
wng_add_header_compile_test(test_header_mutation_summary wng/mutation_summary.hpp)
wng_add_header_compile_test(test_header_serialization_dto wng/serialization_dto.hpp)
wng_add_header_compile_test(test_header_serialization wng/serialization.hpp)
wng_add_header_compile_test(test_header_validation wng/validation.hpp)
wng_add_header_compile_test(test_header_graph wng/graph.hpp)
wng_add_header_compile_test(test_header_wng wng/wng.hpp)
