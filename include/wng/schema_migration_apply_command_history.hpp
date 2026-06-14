// Owns undo and redo stacks for schema migration apply command records.
// This layer is intentionally separate from GraphCommandHistory because migration
// commands are graph-level snapshot transforms rather than GraphCommandRecord batches.

#pragma once

#include <cstddef>
#include <vector>

#include <wng/result.hpp>
#include <wng/schema_migration_apply_command.hpp>

namespace wng
{
    class Graph;

    // Result for schema migration apply command history stack operations. This
    // reports whether stack movement and delegated snapshot restore succeeded.
    struct SchemaMigrationApplyCommandHistoryResult {
        Result result = Result::Ok;

        bool success() const;
    };

    // Minimal undo/redo owner for schema migration apply command records. It does
    // not execute migrations and does not integrate with GraphCommandHistory yet;
    // callers explicitly record successful command-wrapper results.
    class SchemaMigrationApplyCommandHistory {
    public:
        // Records one successful schema migration apply command as an undoable
        // history entry. Recording a new entry clears redo because it starts a new
        // migration-history branch.
        Result record(const SchemaMigrationApplyCommandRecord& record);

        // Clears both undo and redo stacks.
        void clear();

        // Clears only redo entries. This is useful when an owning tool invalidates
        // redo without discarding migration undo history.
        void clear_redo();

        bool can_undo() const;
        bool can_redo() const;

        std::size_t undo_count() const;
        std::size_t redo_count() const;

    private:
        std::vector<SchemaMigrationApplyCommandRecord> undo_stack_;
        std::vector<SchemaMigrationApplyCommandRecord> redo_stack_;

        friend SchemaMigrationApplyCommandHistoryResult undo_last_schema_migration_apply_command(
            Graph& graph,
            SchemaMigrationApplyCommandHistory& history);

        friend SchemaMigrationApplyCommandHistoryResult redo_last_schema_migration_apply_command(
            Graph& graph,
            SchemaMigrationApplyCommandHistory& history);
    };

    // Restores the before snapshot from the most recent migration command record
    // and moves that record to redo only after restore succeeds. This is snapshot
    // based and does not use GraphCommandRecord undo primitives.
    SchemaMigrationApplyCommandHistoryResult undo_last_schema_migration_apply_command(
        Graph& graph,
        SchemaMigrationApplyCommandHistory& history);

    // Restores the after snapshot from the most recent redo migration command
    // record and moves that record back to undo only after restore succeeds.
    SchemaMigrationApplyCommandHistoryResult redo_last_schema_migration_apply_command(
        Graph& graph,
        SchemaMigrationApplyCommandHistory& history);
}
