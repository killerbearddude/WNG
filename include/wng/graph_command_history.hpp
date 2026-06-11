// Owns undo and redo stacks for previously captured graph command records.
// This layer stores successful graph-effect records and delegates graph mutation
// to undo/redo helpers; it does not execute commands or capture editor state.

#pragma once

#include <cstddef>
#include <vector>

#include <wng/graph.hpp>
#include <wng/graph_command.hpp>
#include <wng/graph_redo.hpp>
#include <wng/graph_undo.hpp>
#include <wng/result.hpp>

namespace wng
{
    // Result for history stack operations. The history layer reports only whether
    // stack movement and delegated undo/redo application succeeded.
    struct GraphHistoryResult {
        Result result = Result::Ok;

        bool success() const;
    };

    // Minimal owner for undo and redo stacks of recorded graph command batches.
    // Single command records are normalized to one-record batches so every stack
    // entry represents one user-level graph history step.
    class GraphCommandHistory {
    public:
        // Records one successful command as an undoable history entry.
        // Recording a new entry clears redo because the user has created a new
        // graph branch and the previous redo branch is no longer valid.
        Result record(const GraphCommandRecord& record);

        // Records one successful command batch as a single undoable history entry.
        // Empty or failed batches are rejected because they are not safe history
        // units for later undo/redo application.
        Result record_batch(const GraphCommandBatch& batch);

        // Clears both undo and redo stacks.
        void clear();

        // Clears only redo entries, allowing callers to intentionally invalidate
        // redo without discarding accumulated undo history.
        void clear_redo();

        bool can_undo() const;
        bool can_redo() const;

        std::size_t undo_count() const;
        std::size_t redo_count() const;

    private:
        std::vector<GraphCommandBatch> undo_stack_;
        std::vector<GraphCommandBatch> redo_stack_;

        friend GraphHistoryResult undo_last(
            Graph& graph,
            GraphCommandHistory& history);

        friend GraphHistoryResult redo_last(
            Graph& graph,
            GraphCommandHistory& history);
    };

    // Applies the most recent undo entry and moves it to redo only after graph
    // undo succeeds. On failure, graph atomicity is provided by GraphUndo and the
    // history stacks are left unchanged.
    GraphHistoryResult undo_last(
        Graph& graph,
        GraphCommandHistory& history);

    // Applies the most recent redo entry and moves it back to undo only after
    // graph redo succeeds. This owns stack movement only, not command execution.
    GraphHistoryResult redo_last(
        Graph& graph,
        GraphCommandHistory& history);
}
