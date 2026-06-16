// Exercises GraphCommandTransaction handling for failed batch-level status.
// A composed batch can carry a non-Ok batch result even when its contained graph
// command records succeeded; that state must block commit while preserving
// rollback of the successful graph effects.

#include <cassert>

#include <wng/graph_command_transaction.hpp>

namespace
{
    wng::NodeDesc make_node_desc(const char* title)
    {
        wng::NodeDesc desc;
        desc.type = "transaction.node";
        desc.title = title;
        desc.size = wng::Vec2 { 100.0f, 50.0f };
        return desc;
    }
}

int main()
{
    {
        // The batch-level result is authoritative for commit safety even when no
        // individual record failed. This models composed operations that detect a
        // higher-level failure after successful lower-level graph mutations.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        wng::GraphCommandTransaction transaction;

        const wng::GraphCommandResult created =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(created.result == wng::Result::Ok);
        assert(graph.find_node(created.record.node) != nullptr);

        wng::GraphCommandBatch batch;
        batch.result = wng::Result::InvalidArgument;
        batch.records.push_back(created.record);

        assert(transaction.append_batch(batch) == wng::Result::Ok);
        assert(!transaction.empty());
        assert(transaction.failed());
        assert(!transaction.committable());
        assert(transaction.result() == wng::Result::InvalidArgument);
        assert(transaction.record_count() == 1U);
        assert(transaction.first_failed_command() == nullptr);

        const wng::GraphTransactionResult committed =
            wng::commit_transaction(history, transaction);
        assert(committed.result == wng::Result::InvalidArgument);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);

        const wng::GraphTransactionResult rolled_back =
            wng::rollback_transaction(graph, transaction);
        assert(rolled_back.success());
        assert(graph.find_node(created.record.node) == nullptr);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    return 0;
}
