// Implements mixed graph-level undo/redo stack ownership for WNG graph state.
// GraphHistory preserves user-level chronology across normal command batches and
// schema migration snapshot commands while delegating actual graph mutation to
// existing graph undo/redo and graph snapshot restore primitives.

#include <wng/graph_history.hpp>

#include <new>
#include <vector>

#include <wng/graph.hpp>
#include <wng/graph_redo.hpp>
#include <wng/graph_snapshot.hpp>
#include <wng/graph_undo.hpp>

namespace
{
    wng::GraphHistoryOperationResult history_failure(wng::Result result)
    {
        wng::GraphHistoryOperationResult history_result;
        history_result.result = result;
        return history_result;
    }

    wng::GraphCommandBatch make_single_record_batch(
        const wng::GraphCommandRecord& record)
    {
        wng::GraphCommandBatch batch;
        batch.result = record.result;
        batch.records.push_back(record);
        return batch;
    }

    bool graph_command_batch_is_recordable(
        const wng::GraphCommandBatch& batch)
    {
        if (batch.result != wng::Result::Ok || batch.records.empty()) {
            return false;
        }

        for (const wng::GraphCommandRecord& record : batch.records) {
            if (record.result != wng::Result::Ok) {
                return false;
            }
        }

        return true;
    }

    bool schema_migration_record_is_recordable(
        const wng::SchemaMigrationApplyCommandRecord& record)
    {
        return !record.empty() && record.apply_result.applied();
    }

    wng::Result undo_entry(
        wng::Graph& graph,
        const wng::GraphHistoryEntry& entry)
    {
        switch (entry.kind) {
        case wng::GraphHistoryEntryKind::GraphCommandBatch: {
            const wng::GraphUndoResult undo =
                wng::undo_command_batch(graph, entry.graph_commands);
            return undo.result;
        }
        case wng::GraphHistoryEntryKind::SchemaMigrationApplyCommand:
            return wng::restore_graph_snapshot(graph, entry.schema_migration.before);
        }

        return wng::Result::InvalidArgument;
    }

    wng::Result redo_entry(
        wng::Graph& graph,
        const wng::GraphHistoryEntry& entry)
    {
        switch (entry.kind) {
        case wng::GraphHistoryEntryKind::GraphCommandBatch: {
            const wng::GraphRedoResult redo =
                wng::redo_command_batch(graph, entry.graph_commands);
            return redo.result;
        }
        case wng::GraphHistoryEntryKind::SchemaMigrationApplyCommand:
            // Schema migration redo restores the captured after snapshot instead
            // of rerunning migration policy logic. This keeps redo deterministic
            // even if future schema migration algorithms evolve.
            return wng::restore_graph_snapshot(graph, entry.schema_migration.after);
        }

        return wng::Result::InvalidArgument;
    }
}

namespace wng
{
    bool GraphHistoryEntry::empty() const
    {
        switch (kind) {
        case GraphHistoryEntryKind::GraphCommandBatch:
            return graph_commands.empty();
        case GraphHistoryEntryKind::SchemaMigrationApplyCommand:
            return schema_migration.empty();
        }

        return true;
    }

    bool GraphHistoryOperationResult::success() const
    {
        return result == Result::Ok;
    }

    Result GraphHistory::record_graph_command(const GraphCommandRecord& record)
    {
        if (record.result != Result::Ok) {
            return Result::InvalidArgument;
        }

        try {
            const GraphCommandBatch batch = make_single_record_batch(record);
            return record_graph_command_batch(batch);
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    Result GraphHistory::record_graph_command_batch(const GraphCommandBatch& batch)
    {
        if (!graph_command_batch_is_recordable(batch)) {
            return Result::InvalidArgument;
        }

        try {
            GraphHistoryEntry entry;
            entry.kind = GraphHistoryEntryKind::GraphCommandBatch;
            entry.graph_commands = batch;

            std::vector<GraphHistoryEntry> next_undo = undo_stack_;
            std::vector<GraphHistoryEntry> next_redo;
            next_undo.push_back(entry);

            // Recording a new successful graph command creates a new user branch.
            // Redo is cleared only after all allocations above have succeeded.
            undo_stack_.swap(next_undo);
            redo_stack_.swap(next_redo);
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    Result GraphHistory::record_schema_migration_apply_command(
        const SchemaMigrationApplyCommandRecord& record)
    {
        if (!schema_migration_record_is_recordable(record)) {
            return Result::InvalidArgument;
        }

        try {
            GraphHistoryEntry entry;
            entry.kind = GraphHistoryEntryKind::SchemaMigrationApplyCommand;
            entry.schema_migration = record;

            std::vector<GraphHistoryEntry> next_undo = undo_stack_;
            std::vector<GraphHistoryEntry> next_redo;
            next_undo.push_back(entry);

            // A new migration command invalidates the mixed redo branch just like
            // a normal graph command because both share one user-level timeline.
            undo_stack_.swap(next_undo);
            redo_stack_.swap(next_redo);
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    void GraphHistory::clear()
    {
        undo_stack_.clear();
        redo_stack_.clear();
    }

    void GraphHistory::clear_redo()
    {
        redo_stack_.clear();
    }

    bool GraphHistory::can_undo() const
    {
        return !undo_stack_.empty();
    }

    bool GraphHistory::can_redo() const
    {
        return !redo_stack_.empty();
    }

    std::size_t GraphHistory::undo_count() const
    {
        return undo_stack_.size();
    }

    std::size_t GraphHistory::redo_count() const
    {
        return redo_stack_.size();
    }

    GraphHistoryOperationResult undo_last_graph_history(
        Graph& graph,
        GraphHistory& history)
    {
        if (history.undo_stack_.empty()) {
            return history_failure(Result::NotFound);
        }

        try {
            std::vector<GraphHistoryEntry> next_undo = history.undo_stack_;
            std::vector<GraphHistoryEntry> next_redo = history.redo_stack_;
            const GraphHistoryEntry candidate = next_undo.back();
            next_undo.pop_back();
            next_redo.push_back(candidate);

            // Stack movement is staged before graph mutation so allocation failure
            // cannot occur after the graph has changed. The actual history stacks
            // are swapped only after the delegated undo succeeds.
            const Result undo_result = undo_entry(graph, candidate);
            if (undo_result != Result::Ok) {
                return history_failure(undo_result);
            }

            history.undo_stack_.swap(next_undo);
            history.redo_stack_.swap(next_redo);
            return history_failure(Result::Ok);
        } catch (const std::bad_alloc&) {
            return history_failure(Result::AllocationFailure);
        }
    }

    GraphHistoryOperationResult redo_last_graph_history(
        Graph& graph,
        GraphHistory& history)
    {
        if (history.redo_stack_.empty()) {
            return history_failure(Result::NotFound);
        }

        try {
            std::vector<GraphHistoryEntry> next_undo = history.undo_stack_;
            std::vector<GraphHistoryEntry> next_redo = history.redo_stack_;
            const GraphHistoryEntry candidate = next_redo.back();
            next_redo.pop_back();
            next_undo.push_back(candidate);

            // As with undo, all stack allocations are completed before graph
            // mutation. The visible stack transition happens only after redo or
            // snapshot restoration reports success.
            const Result redo_result = redo_entry(graph, candidate);
            if (redo_result != Result::Ok) {
                return history_failure(redo_result);
            }

            history.undo_stack_.swap(next_undo);
            history.redo_stack_.swap(next_redo);
            return history_failure(Result::Ok);
        } catch (const std::bad_alloc&) {
            return history_failure(Result::AllocationFailure);
        }
    }
}
