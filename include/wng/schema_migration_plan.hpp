// Builds deterministic, read-only migration plans for graph/schema changes.
// This layer explains which graph objects are affected by schema changes but
// does not repair, mutate, or apply migrations.

#pragma once

#include <string>
#include <vector>

#include <wng/ids.hpp>
#include <wng/result.hpp>
#include <wng/schema_compatibility.hpp>
#include <wng/schema_diff.hpp>
#include <wng/schema_migration_policy.hpp>

namespace wng
{
    class Graph;
    class GraphSchema;

    // Describes the kind of migration attention a schema change would require.
    // These are diagnostics only; no action enum value implies automatic repair.
    enum class SchemaMigrationActionKind {
        None,
        RemoveNodeType,
        ModifyNodeType,
        RemovePortDefinition,
        ModifyPortDefinition,
        AddRequiredPort,
        TargetValidationIssue
    };

    // One deterministic migration-planning action. Affected graph IDs are
    // reported in graph storage order so editor diagnostics and tests are stable.
    // Policy coverage is diagnostic only; it marks explicit user/tool intent but
    // does not clear blocking state or apply a migration.
    struct SchemaMigrationAction {
        SchemaMigrationActionKind kind = SchemaMigrationActionKind::None;

        std::string node_type;
        PortKind port_kind = PortKind::Input;
        std::string port_name;

        std::vector<NodeId> affected_nodes;
        std::vector<PortId> affected_ports;

        bool blocking = false;
        bool policy_covered = false;
    };

    // Read-only migration plan for moving graph data between schemas. The
    // embedded compatibility report remains the source for validation status;
    // actions explain the schema changes and affected graph objects.
    struct SchemaMigrationPlan {
        Result result = Result::Ok;

        SchemaCompatibilityReport compatibility;
        std::vector<SchemaMigrationAction> actions;

        bool success() const;
        bool compatible() const;
        bool blocked() const;
        bool empty() const;
    };

    // Builds a deterministic read-only plan for moving graph data from the source
    // schema to the target schema. The plan describes affected graph objects and
    // blocking issues; it never mutates graph or schema state.
    SchemaMigrationPlan build_schema_migration_plan(
        const Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema);

    // Builds a deterministic read-only migration plan and marks actions covered
    // by explicit migration policy entries. Policy coverage does not apply
    // migrations, repair graph objects, or clear blocking state; it only
    // distinguishes known/accepted actions from unresolved actions.
    SchemaMigrationPlan build_schema_migration_plan(
        const Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema,
        const SchemaMigrationPolicy& policy);
}
