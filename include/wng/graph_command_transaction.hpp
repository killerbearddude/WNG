// Builds pending graph command batches before they are committed to history.
// Transactions collect command results produced elsewhere; they do not execute
// commands, capture editor state, or own undo/redo stacks.

#pragma once

#include <cstddef>

#include <wng/graph.hpp>
#include <wng/graph_command.hpp>
#include <wng/graph_command_history.hpp>
#include <wng/graph_history.hpp>
#include <wng/result.hpp>

namespace wng
{
    // Result for transaction commit and rollback operations. Transaction helpers
    // report whether pending command effects were committed to history or rolled
    // back through the existing undo layer.
    struct GraphTransactionResult {
        Result result = Result::Ok;

        bool success() const;
    };

    // Pending batch builder for command results. The transaction layer does not
    // execute graph commands; callers append the results returned by command
    // helpers and then either commit the batch to history or roll it back.
    class GraphCommandTransaction {
    public:
        // Appends one command result to the pending transaction. Successful
        // command records become undoable entries; failed records are retained
        // for diagnostics and make the transaction uncommittable.
        Result append(const GraphCommandResult& result);

        // Appends an existing command batch while preserving record order. This
        // allows a larger pending operation to be composed from smaller batches.
        Result append_batch(const GraphCommandBatch& batch);

        void clear();

        bool empty() const;
        bool failed() const;
        bool committable() const;

        Result result() const;
        std::size_t record_count() const;

        const GraphCommandBatch& batch() const;

        const GraphCommandRecord* first_failed_command() const;

    private:
        GraphCommandBatch batch_;
    };

    // Commits a successful, non-empty transaction to specialized graph command
    // history. This does not mutate Graph; graph mutations already happened when
    // command helpers ran.
    GraphTransactionResult commit_transaction(
        GraphCommandHistory& history,
        const GraphCommandTransaction& transaction);

    // Commits a successful, non-empty transaction to mixed graph-level history.
    // This preserves compatibility with schema migration history ordering while
    // still recording only the command batch produced by the transaction.
    GraphTransactionResult commit_transaction(
        GraphHistory& history,
        const GraphCommandTransaction& transaction);

    // Rolls back successful command effects currently stored in the transaction.
    // Failed records are kept for diagnostics but ignored for rollback because
    // failed command records are not undoable graph effects.
    GraphTransactionResult rollback_transaction(
        Graph& graph,
        const GraphCommandTransaction& transaction);
}
