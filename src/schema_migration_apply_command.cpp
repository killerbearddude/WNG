// Implements graph-level command records for schema migration application.
// The wrapper captures restorable before/after snapshots but deliberately does
// not retrofit schema migrations into GraphCommandRecord or command history yet.

#include <wng/schema_migration_apply_command.hpp>

#include <new>
#include <utility>

#include <wng/graph.hpp>
#include <wng/schema.hpp>

namespace
{
    wng::SchemaMigrationApplyCommandResult command_failure(
        wng::Result result,
        wng::SchemaMigrationApplyCommandStatus status)
    {
        wng::SchemaMigrationApplyCommandResult command_result;
        command_result.result = result;
        command_result.status = status;
        return command_result;
    }
}

namespace wng
{
    bool SchemaMigrationApplyCommandRecord::empty() const
    {
        return before.empty() &&
            after.empty() &&
            diff.empty() &&
            apply_result.command_preview.empty() &&
            apply_result.applied_steps.empty();
    }

    bool SchemaMigrationApplyCommandResult::success() const
    {
        return result == Result::Ok &&
            status == SchemaMigrationApplyCommandStatus::Applied;
    }

    SchemaMigrationApplyCommandResult command_apply_schema_migration(
        Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema,
        const SchemaMigrationPolicy& policy)
    {
        try {
            SchemaMigrationApplyCommandResult result;

            // The command wrapper captures the exact graph state before invoking
            // migration apply. This creates a restorable boundary for future
            // undo/redo without changing how migrations are computed.
            const GraphSnapshotResult before = capture_graph_snapshot(graph);
            if (before.result != Result::Ok) {
                return command_failure(
                    before.result,
                    SchemaMigrationApplyCommandStatus::SnapshotCaptureFailed);
            }

            // Schema migration can rewrite existing node/port metadata, create
            // required ports, and remove obsolete objects. That does not map cleanly
            // to GraphCommandRecord's primitive object mutation record, so this
            // layer records a graph-level before/after snapshot instead.
            SchemaMigrationApplyResult apply = apply_schema_migration(
                graph,
                source_schema,
                target_schema,
                policy);

            if (!apply.applied()) {
                result.result = apply.result;
                result.status = SchemaMigrationApplyCommandStatus::ApplyFailed;
                result.record.before = before.snapshot;
                result.record.apply_result = std::move(apply);
                return result;
            }

            const GraphSnapshotResult after = capture_graph_snapshot(graph);
            if (after.result != Result::Ok) {
                // Post-apply snapshot failure is expected only for exceptional
                // conditions such as allocation failure. The migration has already
                // succeeded, so the best available recovery is to restore the
                // captured before snapshot. The primary failure remains the failed
                // after-snapshot capture; rollback failure cannot be hidden by this
                // result shape and is intentionally not reported as success.
                const Result rollback_result = restore_graph_snapshot(graph, before.snapshot);
                if (rollback_result != Result::Ok) {
                    // Preserve the after-capture failure as the command failure.
                    // The caller can inspect graph state separately if rollback
                    // also failed under the same exceptional conditions.
                }

                result.result = after.result;
                result.status =
                    SchemaMigrationApplyCommandStatus::AfterSnapshotCaptureFailed;
                result.record.before = before.snapshot;
                result.record.apply_result = std::move(apply);
                return result;
            }

            result.result = Result::Ok;
            result.status = SchemaMigrationApplyCommandStatus::Applied;
            result.record.before = before.snapshot;
            result.record.after = after.snapshot;
            result.record.diff = apply.diff;
            result.record.apply_result = std::move(apply);
            return result;
        } catch (const std::bad_alloc&) {
            return command_failure(
                Result::AllocationFailure,
                SchemaMigrationApplyCommandStatus::SnapshotCaptureFailed);
        }
    }
}
