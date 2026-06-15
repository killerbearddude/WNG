// Owns user-level graph history ordering across heterogeneous graph mutations.
// This layer preserves chronological undo/redo order for normal graph command
// batches and schema migration apply command records while delegating graph
// mutation to existing undo/redo and snapshot-restore primitives.

#pragma once

#include <cstddef>
#include <vector>

#include <wng/graph_command.hpp>
#include <wng/result.hpp>
#include <wng/schema_migration_apply_command.hpp>

namespace wng
{
    class Graph;

    // Identifies which existing mutation-record model backs a graph-level
    // history entry. The enum is intentionally limited to graph-core operations;
    // editor state, selection state, and canvas state are not part of this layer.
    enum class GraphHistoryEntryKind {
        GraphCommandBatch,
        SchemaMigrationApplyCommand
    };

    // Stores one user-level undo/redo unit in the mixed graph history stack.
    // Only the payload named by kind is active; the other payload remains a
    // default value so the type stays value-oriented and serialization-neutral.
    struct GraphHistoryEntry {
        GraphHistoryEntryKind kind = GraphHistoryEntryKind::GraphCommandBatch;

        GraphCommandBatch graph_commands;
        SchemaMigrationApplyCommandRecord schema_migration;

        // Reports whether the active payload is empty. This is a structural query
        // only; recordability also checks success status before an entry is stored.
        bool empty() const;
    };

    // Result for graph-level history operations. This reports whether the
    // delegated graph mutation and stack transition completed successfully.
    struct GraphHistoryOperationResult {
        Result result = Result::Ok;

        // Returns true when the delegated graph operation and stack transition
        // completed with Result::Ok.
        bool success() const;
    };

    // Owns a mixed graph-level undo/redo stack for command batches and schema
    // migration apply command records. The class preserves user operation order
    // without owning command execution, migration execution, or editor state.
    class GraphHistory {
    public:
        // Records one successful GraphCommandRecord as a user-level history step.
        // The record is normalized into a one-record batch before storage.
        Result record_graph_command(const GraphCommandRecord& record);

        // Records one successful non-empty GraphCommandBatch as a user-level
        // history step. Recording any new entry clears redo after allocation
        // succeeds because the user has created a new graph history branch.
        Result record_graph_command_batch(const GraphCommandBatch& batch);

        // Records one successful schema migration apply command record. The
        // record is later undone/redone by restoring its captured snapshots, not
        // by rerunning schema migration logic.
        Result record_schema_migration_apply_command(
            const SchemaMigrationApplyCommandRecord& record);

        // Clears both undo and redo stacks without mutating the graph.
        void clear();

        // Clears only redo entries without mutating the graph. Owning tools can
        // use this to invalidate redo after out-of-band graph changes.
        void clear_redo();

        // Reports whether at least one mixed history entry can be undone.
        bool can_undo() const;

        // Reports whether at least one mixed history entry can be redone.
        bool can_redo() const;

        // Returns the number of entries currently available for undo.
        std::size_t undo_count() const;

        // Returns the number of entries currently available for redo.
        std::size_t redo_count() const;

    private:
        std::vector<GraphHistoryEntry> undo_stack_;
        std::vector<GraphHistoryEntry> redo_stack_;

        friend GraphHistoryOperationResult undo_last_graph_history(
            Graph& graph,
            GraphHistory& history);

        friend GraphHistoryOperationResult redo_last_graph_history(
            Graph& graph,
            GraphHistory& history);
    };

    // Applies the most recent mixed graph history entry and moves it to redo only
    // after the delegated graph undo/snapshot restore succeeds. On failure, the
    // history stacks are left unchanged.
    GraphHistoryOperationResult undo_last_graph_history(
        Graph& graph,
        GraphHistory& history);

    // Reapplies the most recent mixed graph history redo entry and moves it back
    // to undo only after the delegated graph redo/snapshot restore succeeds. Schema
    // migrations restore captured after snapshots instead of rerunning migration.
    GraphHistoryOperationResult redo_last_graph_history(
        Graph& graph,
        GraphHistory& history);
}
