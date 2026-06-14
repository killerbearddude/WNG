// Implements undo/redo stack ownership for schema migration apply command records.
// History movement is separated from graph mutation: stacks are prepared first,
// then swapped only after snapshot restoration succeeds.

#include <wng/schema_migration_apply_command_history.hpp>

#include <new>
#include <vector>

#include <wng/graph.hpp>
#include <wng/graph_snapshot.hpp>

namespace
{
    wng::SchemaMigrationApplyCommandHistoryResult history_failure(wng::Result result)
    {
        wng::SchemaMigrationApplyCommandHistoryResult history_result;
        history_result.result = result;
        return history_result;
    }

    bool record_is_recordable(const wng::SchemaMigrationApplyCommandRecord& record)
    {
        return !record.empty() && record.apply_result.applied();
    }
}

namespace wng
{
    bool SchemaMigrationApplyCommandHistoryResult::success() const
    {
        return result == Result::Ok;
    }

    Result SchemaMigrationApplyCommandHistory::record(
        const SchemaMigrationApplyCommandRecord& record)
    {
        if (!record_is_recordable(record)) {
            return Result::InvalidArgument;
        }

        try {
            std::vector<SchemaMigrationApplyCommandRecord> next_undo = undo_stack_;
            std::vector<SchemaMigrationApplyCommandRecord> next_redo;
            next_undo.push_back(record);

            // Recording a new successful migration command creates a new history
            // branch. Redo is cleared only after all allocations have succeeded.
            undo_stack_.swap(next_undo);
            redo_stack_.swap(next_redo);
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    void SchemaMigrationApplyCommandHistory::clear()
    {
        undo_stack_.clear();
        redo_stack_.clear();
    }

    void SchemaMigrationApplyCommandHistory::clear_redo()
    {
        redo_stack_.clear();
    }

    bool SchemaMigrationApplyCommandHistory::can_undo() const
    {
        return !undo_stack_.empty();
    }

    bool SchemaMigrationApplyCommandHistory::can_redo() const
    {
        return !redo_stack_.empty();
    }

    std::size_t SchemaMigrationApplyCommandHistory::undo_count() const
    {
        return undo_stack_.size();
    }

    std::size_t SchemaMigrationApplyCommandHistory::redo_count() const
    {
        return redo_stack_.size();
    }

    SchemaMigrationApplyCommandHistoryResult undo_last_schema_migration_apply_command(
        Graph& graph,
        SchemaMigrationApplyCommandHistory& history)
    {
        if (history.undo_stack_.empty()) {
            return history_failure(Result::NotFound);
        }

        try {
            std::vector<SchemaMigrationApplyCommandRecord> next_undo = history.undo_stack_;
            std::vector<SchemaMigrationApplyCommandRecord> next_redo = history.redo_stack_;
            const SchemaMigrationApplyCommandRecord candidate = next_undo.back();
            next_undo.pop_back();
            next_redo.push_back(candidate);

            // Migration command undo is snapshot based. Stack movement is committed
            // only after the graph has been restored to the recorded before state.
            const Result restore_result = restore_graph_snapshot(graph, candidate.before);
            if (restore_result != Result::Ok) {
                return history_failure(restore_result);
            }

            history.undo_stack_.swap(next_undo);
            history.redo_stack_.swap(next_redo);
            return history_failure(Result::Ok);
        } catch (const std::bad_alloc&) {
            return history_failure(Result::AllocationFailure);
        }
    }

    SchemaMigrationApplyCommandHistoryResult redo_last_schema_migration_apply_command(
        Graph& graph,
        SchemaMigrationApplyCommandHistory& history)
    {
        if (history.redo_stack_.empty()) {
            return history_failure(Result::NotFound);
        }

        try {
            std::vector<SchemaMigrationApplyCommandRecord> next_undo = history.undo_stack_;
            std::vector<SchemaMigrationApplyCommandRecord> next_redo = history.redo_stack_;
            const SchemaMigrationApplyCommandRecord candidate = next_redo.back();
            next_redo.pop_back();
            next_undo.push_back(candidate);

            // Redo restores the recorded after snapshot instead of replaying the
            // migration algorithm. This keeps redo deterministic even if future
            // schema policy logic evolves.
            const Result restore_result = restore_graph_snapshot(graph, candidate.after);
            if (restore_result != Result::Ok) {
                return history_failure(restore_result);
            }

            history.undo_stack_.swap(next_undo);
            history.redo_stack_.swap(next_redo);
            return history_failure(Result::Ok);
        } catch (const std::bad_alloc&) {
            return history_failure(Result::AllocationFailure);
        }
    }
}
