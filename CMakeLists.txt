cmake_minimum_required(VERSION 3.16)

project(WNG VERSION 0.1.0 LANGUAGES CXX)

add_library(wng_core
    src/graph.cpp
    src/validation.cpp
    src/mutation_summary.cpp
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
