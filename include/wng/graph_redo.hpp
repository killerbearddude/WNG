// Reapplies graph mutations from previously captured command records.
// This layer is the redo primitive for future history systems; it does not own
// an undo stack, redo stack, editor state, or command replay log.

#pragma once

#include <vector>

#include <wng/graph.hpp>
#include <wng/graph_command.hpp>
#include <wng/result.hpp>

namespace wng
{
    // Result of reapplying graph mutations. applied_records is populated only
    // after the entire redo operation succeeds, preserving atomic semantics.
    struct GraphRedoResult {
        Result result = Result::Ok;
        std::vector<GraphCommandRecord> applied_records;

        bool success() const;
    };

    // Reapplies the graph effect of one successful command record.
    // Created snapshots are restored with stable IDs; removed snapshots are
    // removed again through Graph APIs.
    GraphRedoResult redo_command(
        Graph& graph,
        const GraphCommandRecord& record);

    // Reapplies a successful command batch in original record order.
    // The batch itself is metadata only; this helper performs the graph mutations
    // needed by future redo history.
    GraphRedoResult redo_command_batch(
        Graph& graph,
        const GraphCommandBatch& batch);
}
