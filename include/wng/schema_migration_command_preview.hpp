// Previews prospective graph-level operations for schema migration.
// This layer is read-only and intentionally does not produce GraphCommandRecord
// values because no graph mutation has occurred.

#pragma once

#include <string>
#include <vector>

#include <wng/ids.hpp>
#include <wng/result.hpp>
#include <wng/schema_migration_apply_preview.hpp>
#include <wng/schema_migration_policy.hpp>

namespace wng
{
    class Graph;
    class GraphSchema;

    // High-level outcome for command preview generation. These statuses describe
    // whether prospective operations could be derived without applying them.
    enum class SchemaMigrationCommandPreviewStatus {
        Ready,
        PolicyInvalid,
        PlanFailed,
        BlockedByUncoveredActions,
        NoPreviewableOperations
    };

    // Describes the kind of speculative graph operation a future migration apply
    // layer might need to perform. No enum value performs a mutation here.
    enum class SchemaMigrationCommandPreviewStepKind {
        RenameNodeType,
        RenamePortDefinition,
        ChangePortType,
        AddRequiredPort,
        RemoveNodesForRemovedType,
        RemovePortsForRemovedDefinition
    };

    // One deterministic prospective migration operation. Affected IDs are copied
    // from the current graph in graph storage order for stable diagnostics.
    struct SchemaMigrationCommandPreviewStep {
        SchemaMigrationCommandPreviewStepKind kind =
            SchemaMigrationCommandPreviewStepKind::RenameNodeType;

        std::string from_node_type;
        std::string to_node_type;

        PortDefinitionIdentity from_port;
        PortDefinitionIdentity to_port;

        std::string from_type;
        std::string to_type;
        std::string default_value;

        std::vector<NodeId> affected_nodes;
        std::vector<PortId> affected_ports;

        bool destructive = false;
        bool policy_covered = false;
    };

    // Read-only command preview. It embeds the apply preview used to derive the
    // step list and never stores command records or undo/redo payloads.
    struct SchemaMigrationCommandPreview {
        Result result = Result::Ok;
        SchemaMigrationCommandPreviewStatus status =
            SchemaMigrationCommandPreviewStatus::Ready;

        SchemaMigrationApplyPreview apply_preview;
        std::vector<SchemaMigrationCommandPreviewStep> steps;

        bool success() const;
        bool ready() const;
        bool blocked() const;
        bool empty() const;
    };

    // Builds a deterministic read-only preview of prospective migration
    // operations. The preview consumes schema-aware policy validation and
    // policy-aware migration planning, but never mutates graph or schema state.
    SchemaMigrationCommandPreview preview_schema_migration_commands(
        const Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema,
        const SchemaMigrationPolicy& policy);
}
