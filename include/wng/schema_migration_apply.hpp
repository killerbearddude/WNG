// Applies policy-covered schema migrations to graphs.
// The implementation uses an in-memory DTO working copy and replaces the graph
// only after the migrated result validates against the target schema.

#pragma once

#include <vector>

#include <wng/graph_diff.hpp>
#include <wng/result.hpp>
#include <wng/schema_migration_command_preview.hpp>

namespace wng
{
    class Graph;
    class GraphSchema;

    // High-level outcome for schema migration application. UnsupportedDestructiveOperation
    // is reserved for destructive preview steps not handled by current apply semantics.
    enum class SchemaMigrationApplyStatus {
        Applied,
        PreviewFailed,
        NotReady,
        UnsupportedDestructiveOperation,
        TargetValidationFailed
    };

    // Records one command-preview step that was applied to the DTO working copy.
    // The preview step is preserved for diagnostics and future command wrapping.
    struct SchemaMigrationAppliedStep {
        SchemaMigrationCommandPreviewStep preview_step;
        bool applied = false;
    };

    // Result of an atomic schema migration application attempt. The command
    // preview explains the intended operations and diff reports the final graph
    // change only when replacement succeeds.
    struct SchemaMigrationApplyResult {
        Result result = Result::Ok;
        SchemaMigrationApplyStatus status = SchemaMigrationApplyStatus::Applied;

        SchemaMigrationCommandPreview command_preview;
        std::vector<SchemaMigrationAppliedStep> applied_steps;

        GraphDiff diff;

        bool success() const;
        bool applied() const;
    };

    // Applies policy-covered migration operations described by policy and target
    // schema. The source graph is unchanged on failure. Destructive removals are
    // supported only when represented by explicit policy-covered preview steps.
    SchemaMigrationApplyResult apply_schema_migration(
        Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema,
        const SchemaMigrationPolicy& policy);
}
