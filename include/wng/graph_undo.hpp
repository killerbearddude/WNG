// Applies inverse graph mutations from previously captured command records.
// This layer is the undo primitive for future history systems; it does not own
// an undo stack, redo stack, editor state, or command replay log.

#pragma once

#include <vector>

#include <wng/graph.hpp>
#include <wng/graph_command.hpp>
#include <wng/result.hpp>

namespace wng
{
    // Result of applying inverse graph mutations. applied_records is populated
    // only after the entire undo operation succeeds, preserving atomic semantics.
    struct GraphUndoResult {
        Result result = Result::Ok;
        std::vector<GraphCommandRecord> applied_records;

        bool success() const;
    };

    // Applies the inverse of one successful command record.
    // Created objects are removed through Graph APIs; removed objects are restored
    // through graph_restore so their stable IDs are preserved.
    GraphUndoResult undo_command(
        Graph& graph,
        const GraphCommandRecord& record);

    // Applies the inverse of a successful command batch in reverse record order.
    // The batch itself is metadata only; this helper performs the actual inverse
    // graph mutations needed by future undo history.
    GraphUndoResult undo_command_batch(
        Graph& graph,
        const GraphCommandBatch& batch);
}
