// Implements pending graph command transactions for WNG.
// A transaction is a batch builder around already-executed command results: it
// does not execute commands, own history stacks, or capture editor state.

#include <new>

#include <wng/graph_command_transaction.hpp>

#include <wng/graph_undo.hpp>

namespace
{
    wng::GraphTransactionResult transaction_failure(wng::Result result)
    {
        wng::GraphTransactionResult transaction_result;
        transaction_result.result = result;
        return transaction_result;
    }

    bool has_successful_records(const wng::GraphCommandBatch& batch)
    {
        for (const wng::GraphCommandRecord& record : batch.records) {
            if (record.result == wng::Result::Ok) {
                return true;
            }
        }

        return false;
    }

    wng::GraphCommandBatch successful_records_only(
        const wng::GraphCommandBatch& batch)
    {
        wng::GraphCommandBatch successful;

        // Rollback is for aborting a pending operation. Failed records are
        // diagnostic data only, so only successful command effects are passed to
        // the undo primitive.
        for (const wng::GraphCommandRecord& record : batch.records) {
            if (record.result == wng::Result::Ok) {
                successful.records.push_back(record);
            }
        }

        return successful;
    }
}

namespace wng
{
    bool GraphTransactionResult::success() const
    {
        return result == Result::Ok;
    }

    Result GraphCommandTransaction::append(const GraphCommandResult& result)
    {
        try {
            // Transactions do not execute commands. They only retain command
            // results produced by the explicit graph command helpers.
            append_command_result(batch_, result);
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    Result GraphCommandTransaction::append_batch(const GraphCommandBatch& batch)
    {
        if (batch.records.empty()) {
            return Result::InvalidArgument;
        }

        try {
            for (const GraphCommandRecord& record : batch.records) {
                batch_.records.push_back(record);

                // Failed records are retained for diagnostics. The first failure
                // controls transaction.result() so callers can report the root
                // command failure without scanning the whole batch.
                if (batch_.result == Result::Ok && record.result != Result::Ok) {
                    batch_.result = record.result;
                }
            }

            if (batch_.result == Result::Ok && batch.result != Result::Ok) {
                batch_.result = batch.result;
            }

            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    void GraphCommandTransaction::clear()
    {
        batch_ = GraphCommandBatch {};
    }

    bool GraphCommandTransaction::empty() const
    {
        return batch_.records.empty();
    }

    bool GraphCommandTransaction::failed() const
    {
        return batch_.result != Result::Ok;
    }

    bool GraphCommandTransaction::committable() const
    {
        return !empty() && !failed();
    }

    Result GraphCommandTransaction::result() const
    {
        return batch_.result;
    }

    std::size_t GraphCommandTransaction::record_count() const
    {
        return batch_.records.size();
    }

    const GraphCommandBatch& GraphCommandTransaction::batch() const
    {
        return batch_;
    }

    const GraphCommandRecord* GraphCommandTransaction::first_failed_command() const
    {
        for (const GraphCommandRecord& record : batch_.records) {
            if (record.result != Result::Ok) {
                return &record;
            }
        }

        return nullptr;
    }

    GraphTransactionResult commit_transaction(
        GraphCommandHistory& history,
        const GraphCommandTransaction& transaction)
    {
        if (!transaction.committable()) {
            return transaction_failure(Result::InvalidArgument);
        }

        try {
            // Commit transfers a completed user-level graph operation into the
            // specialized command history owner. It does not mutate Graph because
            // command helpers already performed the graph mutations before append().
            return transaction_failure(history.record_batch(transaction.batch()));
        } catch (const std::bad_alloc&) {
            return transaction_failure(Result::AllocationFailure);
        }
    }

    GraphTransactionResult commit_transaction(
        GraphHistory& history,
        const GraphCommandTransaction& transaction)
    {
        if (!transaction.committable()) {
            return transaction_failure(Result::InvalidArgument);
        }

        try {
            // The mixed graph-level history owns user chronology across command
            // batches and schema migration records. A transaction contributes only
            // its already-executed graph command batch.
            return transaction_failure(
                history.record_graph_command_batch(transaction.batch()));
        } catch (const std::bad_alloc&) {
            return transaction_failure(Result::AllocationFailure);
        }
    }

    GraphTransactionResult rollback_transaction(
        Graph& graph,
        const GraphCommandTransaction& transaction)
    {
        try {
            const GraphCommandBatch successful = successful_records_only(transaction.batch());
            if (!has_successful_records(successful)) {
                return GraphTransactionResult {};
            }

            const GraphUndoResult undo_result = undo_command_batch(graph, successful);
            return transaction_failure(undo_result.result);
        } catch (const std::bad_alloc&) {
            return transaction_failure(Result::AllocationFailure);
        }
    }
}
