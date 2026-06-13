// Wraps schema migration application in graph-level command records.
// This layer captures before/after graph snapshots for future history integration
// without changing schema migration application semantics.

#pragma once

#include <wng/graph_diff.hpp>
#include <wng/graph_snapshot.hpp>
#include <wng/result.hpp>
#include <wng/schema_migration_apply.hpp>

namespace wng
{
    class Graph;
    class GraphSchema;

    // High-level outcome for command-wrapped schema migration application.
    // The status reports which boundary failed: snapshot capture, migration
    // application, or post-apply snapshot capture.
    enum class SchemaMigrationApplyCommandStatus {
        Applied,
        SnapshotCaptureFailed,
        ApplyFailed,
        AfterSnapshotCaptureFailed
    };

    // Graph-level migration command record. It stores restorable snapshots around
    // the completed migration instead of forcing schema metadata rewrites into
    // GraphCommandRecord's object-creation/removal model.
    struct SchemaMigrationApplyCommandRecord {
        GraphSnapshot before;
        GraphSnapshot after;

        SchemaMigrationApplyResult apply_result;

        GraphDiff diff;

        bool empty() const;
    };

    // Result of applying a schema migration through the command wrapper. On
    // success, record contains before/after snapshots and the preserved apply
    // result. On failure, status identifies the boundary that failed.
    struct SchemaMigrationApplyCommandResult {
        Result result = Result::Ok;
        SchemaMigrationApplyCommandStatus status =
            SchemaMigrationApplyCommandStatus::Applied;

        SchemaMigrationApplyCommandRecord record;

        bool success() const;
    };

    // Applies a schema migration and captures before/after snapshots for future
    // history integration. The migration itself remains atomic through
    // apply_schema_migration; this wrapper only records the graph-level change.
    SchemaMigrationApplyCommandResult command_apply_schema_migration(
        Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema,
        const SchemaMigrationPolicy& policy);
}
