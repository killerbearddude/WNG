// Implements minimal undo/redo stack ownership for WNG graph commands.
// History owns recorded batches only; command execution, editor state, and
// automatic recording remain outside this layer.

#include <new>
#include <vector>

#include <wng/graph_command_history.hpp>

namespace
{
    wng::GraphHistoryResult history_failure(wng::Result result)
    {
        wng::GraphHistoryResult history_result;
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

    bool batch_is_recordable(const wng::GraphCommandBatch& batch)
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
}

namespace wng
{
    bool GraphHistoryResult::success() const
    {
        return result == Result::Ok;
    }

    Result GraphCommandHistory::record(const GraphCommandRecord& record)
    {
        if (record.result != Result::Ok) {
            return Result::InvalidArgument;
        }

        try {
            const GraphCommandBatch batch = make_single_record_batch(record);
            return record_batch(batch);
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    Result GraphCommandHistory::record_batch(const GraphCommandBatch& batch)
    {
        if (!batch_is_recordable(batch)) {
            return Result::InvalidArgument;
        }

        try {
            std::vector<GraphCommandBatch> next_undo = undo_stack_;
            std::vector<GraphCommandBatch> next_redo;
            next_undo.push_back(batch);

            // Recording a new successful graph action starts a new history branch.
            // Redo is invalidated only after all allocations above have succeeded,
            // so allocation failure leaves both stacks unchanged.
            undo_stack_.swap(next_undo);
            redo_stack_.swap(next_redo);
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    void GraphCommandHistory::clear()
    {
        undo_stack_.clear();
        redo_stack_.clear();
    }

    void GraphCommandHistory::clear_redo()
    {
        redo_stack_.clear();
    }

    bool GraphCommandHistory::can_undo() const
    {
        return !undo_stack_.empty();
    }

    bool GraphCommandHistory::can_redo() const
    {
        return !redo_stack_.empty();
    }

    std::size_t GraphCommandHistory::undo_count() const
    {
        return undo_stack_.size();
    }

    std::size_t GraphCommandHistory::redo_count() const
    {
        return redo_stack_.size();
    }

    GraphHistoryResult undo_last(
        Graph& graph,
        GraphCommandHistory& history)
    {
        if (history.undo_stack_.empty()) {
            return history_failure(Result::NotFound);
        }

        try {
            std::vector<GraphCommandBatch> next_undo = history.undo_stack_;
            std::vector<GraphCommandBatch> next_redo = history.redo_stack_;
            const GraphCommandBatch candidate = next_undo.back();
            next_undo.pop_back();
            next_redo.push_back(candidate);

            // Stack transition atomicity is separated from graph atomicity: all
            // stack allocations happen before graph mutation, and stacks are
            // swapped only after the undo primitive reports success.
            const GraphUndoResult undo_result = undo_command_batch(graph, candidate);
            if (!undo_result.success()) {
                return history_failure(undo_result.result);
            }

            history.undo_stack_.swap(next_undo);
            history.redo_stack_.swap(next_redo);
            return history_failure(Result::Ok);
        } catch (const std::bad_alloc&) {
            return history_failure(Result::AllocationFailure);
        }
    }

    GraphHistoryResult redo_last(
        Graph& graph,
        GraphCommandHistory& history)
    {
        if (history.redo_stack_.empty()) {
            return history_failure(Result::NotFound);
        }

        try {
            std::vector<GraphCommandBatch> next_undo = history.undo_stack_;
            std::vector<GraphCommandBatch> next_redo = history.redo_stack_;
            const GraphCommandBatch candidate = next_redo.back();
            next_redo.pop_back();
            next_undo.push_back(candidate);

            // Redo uses the redo primitive, which applies batch records in their
            // original order. History only moves the entry back to undo after the
            // graph effect has been reapplied successfully.
            const GraphRedoResult redo_result = redo_command_batch(graph, candidate);
            if (!redo_result.success()) {
                return history_failure(redo_result.result);
            }

            history.undo_stack_.swap(next_undo);
            history.redo_stack_.swap(next_redo);
            return history_failure(Result::Ok);
        } catch (const std::bad_alloc&) {
            return history_failure(Result::AllocationFailure);
        }
    }
}
