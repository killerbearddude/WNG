cmake_minimum_required(VERSION 3.16)

project(WNG VERSION 0.1.0 LANGUAGES CXX)

add_library(wng_core
    src/graph.cpp
    src/validation.cpp
    src/mutation_summary.cpp
    src/serialization.cpp
    src/schema.cpp
    src/schema_validation.cpp
    src/schema_mutation.cpp
    src/graph_validation.cpp
    src/graph_traversal.cpp
    src/dirty_propagation.cpp
    src/execution_plan.cpp
    src/graph_command.cpp
    src/graph_restore.cpp
    src/graph_undo.cpp
    src/graph_redo.cpp
    src/graph_command_history.cpp
    src/graph_history.cpp
    src/graph_session.cpp
    src/graph_editor_state.cpp
    src/graph_editor_state_cleanup.cpp
    src/graph_editor_selection_commands.cpp
    src/graph_hit_testing.cpp
    src/graph_editor_hit_testing.cpp
    src/graph_command_transaction.cpp
    src/graph_diff.cpp
    src/graph_mutation_preview.cpp
    src/graph_snapshot.cpp
    src/schema_snapshot.cpp
    src/schema_diff.cpp
    src/schema_compatibility.cpp
    src/schema_migration_plan.cpp
    src/schema_migration_policy.cpp
    src/schema_migration_apply_preview.cpp
    src/schema_migration_command_preview.cpp
    src/schema_migration_apply.cpp
    src/schema_migration_apply_command.cpp
    src/schema_migration_apply_command_history.cpp
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

function(wng_keep_assertions_enabled name)
    # WNG tests intentionally use assert() for checks and fixture setup.
    # Keep those assertions active in every test binary configuration so
    # release-like builds validate the same test semantics as Debug without
    # changing wng_core flags.
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${name} PRIVATE -UNDEBUG)
    elseif (MSVC)
        target_compile_options(${name} PRIVATE /UNDEBUG)
    endif()
endfunction()

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
    wng_keep_assertions_enabled(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

wng_add_test(test_assertions_enabled tests/test_assertions_enabled.cpp)
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
wng_add_test(test_import_atomic tests/test_import_atomic.cpp)
wng_add_test(test_schema tests/test_schema.cpp)
wng_add_test(test_schema_validation tests/test_schema_validation.cpp)
wng_add_test(test_schema_mutation tests/test_schema_mutation.cpp)
wng_add_test(test_schema_node_port_mutation tests/test_schema_node_port_mutation.cpp)
wng_add_test(test_schema_node_instantiation tests/test_schema_node_instantiation.cpp)
wng_add_test(test_graph_validation tests/test_graph_validation.cpp)
wng_add_test(test_validation_order tests/test_validation_order.cpp)
wng_add_test(test_graph_validation_schema_callbacks tests/test_graph_validation_schema_callbacks.cpp)
wng_add_test(test_graph_validation_resource_exhaustion tests/test_graph_validation_resource_exhaustion.cpp)
wng_add_test(test_graph_traversal tests/test_graph_traversal.cpp)
wng_add_test(test_dirty_propagation tests/test_dirty_propagation.cpp)
wng_add_test(test_execution_plan tests/test_execution_plan.cpp)
wng_add_test(test_graph_core_workflow tests/test_graph_core_workflow.cpp)
wng_add_test(test_graph_command tests/test_graph_command.cpp)
wng_add_test(test_schema_graph_command tests/test_schema_graph_command.cpp)
wng_add_test(test_graph_restore tests/test_graph_restore.cpp)
wng_add_test(test_restore_atomic tests/test_restore_atomic.cpp)
wng_add_test(test_graph_undo tests/test_graph_undo.cpp)
wng_add_test(test_graph_redo tests/test_graph_redo.cpp)
wng_add_test(test_graph_command_history tests/test_graph_command_history.cpp)
wng_add_test(test_graph_command_history_batch_invariants tests/test_graph_command_history_batch_invariants.cpp)
wng_add_test(test_graph_history tests/test_graph_history.cpp)
wng_add_test(test_graph_history_redo_invariants tests/test_graph_history_redo_invariants.cpp)
wng_add_test(test_graph_history_clear_state tests/test_graph_history_clear_state.cpp)
wng_add_test(test_graph_history_record_invariants tests/test_graph_history_record_invariants.cpp)
wng_add_test(test_graph_command_transaction tests/test_graph_command_transaction.cpp)
wng_add_test(test_graph_command_transaction_batch_status tests/test_graph_command_transaction_batch_status.cpp)
wng_add_test(test_graph_diff tests/test_graph_diff.cpp)
wng_add_test(test_diff_order tests/test_diff_order.cpp)
wng_add_test(test_graph_mutation_preview tests/test_graph_mutation_preview.cpp)
wng_add_test(test_graph_snapshot tests/test_graph_snapshot.cpp)
wng_add_test(test_graph_snapshot_high_ids tests/test_graph_snapshot_high_ids.cpp)
wng_add_test(test_snapshot_diff_failure tests/test_snapshot_diff_failure.cpp)
wng_add_test(test_schema_snapshot tests/test_schema_snapshot.cpp)
wng_add_test(test_schema_diff tests/test_schema_diff.cpp)
wng_add_test(test_schema_compatibility tests/test_schema_compatibility.cpp)
wng_add_test(test_schema_migration_plan tests/test_schema_migration_plan.cpp)
wng_add_test(test_schema_migration_policy tests/test_schema_migration_policy.cpp)
wng_add_test(test_schema_migration_policy_schema tests/test_schema_migration_policy_schema.cpp)
wng_add_test(test_schema_migration_apply_preview tests/test_schema_migration_apply_preview.cpp)
wng_add_test(test_schema_migration_command_preview tests/test_schema_migration_command_preview.cpp)
wng_add_test(test_schema_migration_apply tests/test_schema_migration_apply.cpp)
wng_add_test(test_schema_migration_apply_destructive tests/test_schema_migration_apply_destructive.cpp)
wng_add_test(test_schema_migration_apply_command tests/test_schema_migration_apply_command.cpp)
wng_add_test(test_schema_migration_apply_command_history tests/test_schema_migration_apply_command_history.cpp)
wng_add_test(test_schema_history_record tests/test_schema_history_record.cpp)
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

    wng_keep_assertions_enabled(${name})
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
wng_add_header_compile_test(test_header_schema wng/schema.hpp)
wng_add_header_compile_test(test_header_schema_validation wng/schema_validation.hpp)
wng_add_header_compile_test(test_header_schema_mutation wng/schema_mutation.hpp)
wng_add_header_compile_test(test_header_graph_validation wng/graph_validation.hpp)
wng_add_header_compile_test(test_header_graph_traversal wng/graph_traversal.hpp)
wng_add_header_compile_test(test_header_dirty_propagation wng/dirty_propagation.hpp)
wng_add_header_compile_test(test_header_execution_plan wng/execution_plan.hpp)
wng_add_header_compile_test(test_header_graph_command wng/graph_command.hpp)
wng_add_header_compile_test(test_header_graph_restore wng/graph_restore.hpp)
wng_add_header_compile_test(test_header_graph_undo wng/graph_undo.hpp)
wng_add_header_compile_test(test_header_graph_redo wng/graph_redo.hpp)
wng_add_header_compile_test(test_header_graph_command_history wng/graph_command_history.hpp)
wng_add_header_compile_test(test_header_graph_history wng/graph_history.hpp)
wng_add_header_compile_test(test_header_graph_session wng/graph_session.hpp)
wng_add_header_compile_test(test_header_graph_editor_state wng/graph_editor_state.hpp)
wng_add_header_compile_test(test_header_graph_editor_state_cleanup wng/graph_editor_state_cleanup.hpp)
wng_add_header_compile_test(test_header_graph_editor_selection_commands wng/graph_editor_selection_commands.hpp)
wng_add_header_compile_test(test_header_graph_hit_testing wng/graph_hit_testing.hpp)
wng_add_header_compile_test(test_header_graph_editor_hit_testing wng/graph_editor_hit_testing.hpp)
wng_add_header_compile_test(test_header_graph_command_transaction wng/graph_command_transaction.hpp)
wng_add_header_compile_test(test_header_graph_diff wng/graph_diff.hpp)
wng_add_header_compile_test(test_header_graph_mutation_preview wng/graph_mutation_preview.hpp)
wng_add_header_compile_test(test_header_graph_snapshot wng/graph_snapshot.hpp)
wng_add_header_compile_test(test_header_schema_snapshot wng/schema_snapshot.hpp)
wng_add_header_compile_test(test_header_schema_diff wng/schema_diff.hpp)
wng_add_header_compile_test(test_header_schema_compatibility wng/schema_compatibility.hpp)
wng_add_header_compile_test(test_header_schema_migration_plan wng/schema_migration_plan.hpp)
wng_add_header_compile_test(test_header_schema_migration_policy wng/schema_migration_policy.hpp)
wng_add_header_compile_test(test_header_schema_migration_apply_preview wng/schema_migration_apply_preview.hpp)
wng_add_header_compile_test(test_header_schema_migration_command_preview wng/schema_migration_command_preview.hpp)
wng_add_header_compile_test(test_header_schema_migration_apply wng/schema_migration_apply.hpp)
wng_add_header_compile_test(test_header_schema_migration_apply_command wng/schema_migration_apply_command.hpp)
wng_add_header_compile_test(test_header_schema_migration_apply_command_history wng/schema_migration_apply_command_history.hpp)
wng_add_header_compile_test(test_header_validation wng/validation.hpp)
wng_add_header_compile_test(test_header_graph wng/graph.hpp)
wng_add_header_compile_test(test_header_wng wng/wng.hpp)
