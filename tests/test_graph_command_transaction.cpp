// Exercises pending command transaction behavior.
// These tests verify batch-building, commit, and rollback semantics without
// adding editor transactions, automatic command execution, or WPL integration.

#include <cassert>
#include <vector>

#include <wng/graph_command_transaction.hpp>

namespace
{
    wng::NodeDesc make_node_desc(const char* title = "Node")
    {
        wng::NodeDesc desc;
        desc.type = "test.node";
        desc.title = title;
        desc.size = wng::Vec2 { 100.0f, 50.0f };
        return desc;
    }

    wng::PortDesc make_port_desc(
        wng::PortKind kind,
        const char* name,
        const char* type = "number")
    {
        wng::PortDesc desc;
        desc.kind = kind;
        desc.name = name;
        desc.type = type;
        return desc;
    }

    wng::NodeId create_node(wng::Graph& graph, const char* title = "Node")
    {
        const wng::GraphCommandResult result =
            wng::command_create_node(graph, make_node_desc(title));
        assert(result.result == wng::Result::Ok);
        return result.record.node;
    }

    wng::GraphCommandResult failed_add_port(wng::Graph& graph)
    {
        return wng::command_add_port(
            graph,
            wng::NodeId { 999 },
            make_port_desc(wng::PortKind::Input, "missing"));
    }

    wng::GraphCommandResult failed_link(wng::Graph& graph)
    {
        const wng::NodeId node = create_node(graph, "Invalid link node");
        const wng::GraphCommandResult input_a = wng::command_add_port(
            graph,
            node,
            make_port_desc(wng::PortKind::Input, "a"));
        const wng::GraphCommandResult input_b = wng::command_add_port(
            graph,
            node,
            make_port_desc(wng::PortKind::Input, "b"));
        assert(input_a.result == wng::Result::Ok);
        assert(input_b.result == wng::Result::Ok);

        return wng::command_create_link(
            graph,
            input_a.record.port,
            input_b.record.port);
    }

    wng::GraphCommandBatch make_batch(
        const std::vector<wng::GraphCommandRecord>& records)
    {
        wng::GraphCommandBatch batch;
        for (const wng::GraphCommandRecord& record : records) {
            wng::GraphCommandResult result;
            result.result = record.result;
            result.record = record;
            wng::append_command_result(batch, result);
        }

        return batch;
    }
}

int main()
{
    {
        // Verifies the initial transaction state. Empty transactions are useful
        // as pending builders but must not be committable history entries.
        const wng::GraphCommandTransaction transaction;

        assert(transaction.empty());
        assert(!transaction.failed());
        assert(!transaction.committable());
        assert(transaction.record_count() == 0U);
        assert(transaction.result() == wng::Result::Ok);
        assert(transaction.first_failed_command() == nullptr);
    }

    {
        // Verifies that appending a successful command result makes the
        // transaction a valid pending history step without extra graph mutation.
        wng::Graph graph;
        wng::GraphCommandTransaction transaction;
        const wng::GraphCommandResult created =
            wng::command_create_node(graph, make_node_desc("A"));

        assert(transaction.append(created) == wng::Result::Ok);
        assert(!transaction.empty());
        assert(!transaction.failed());
        assert(transaction.committable());
        assert(transaction.record_count() == 1U);
        assert(transaction.batch().records.size() == 1U);
        assert(transaction.batch().records[0].node == created.record.node);
    }

    {
        // Verifies that failed command records are retained for diagnostics while
        // preventing the transaction from being committed to history.
        wng::Graph graph;
        wng::GraphCommandTransaction transaction;
        const wng::GraphCommandResult failed = failed_add_port(graph);

        assert(transaction.append(failed) == wng::Result::Ok);
        assert(!transaction.empty());
        assert(transaction.failed());
        assert(!transaction.committable());
        assert(transaction.result() == wng::Result::NotFound);
        assert(transaction.first_failed_command() != nullptr);
        assert(transaction.first_failed_command()->result == wng::Result::NotFound);
    }

    {
        // Verifies first-failure preservation. Future editor workflows can report
        // the original command failure even if later appended commands also fail.
        wng::Graph graph;
        wng::GraphCommandTransaction transaction;
        const wng::GraphCommandResult first = failed_add_port(graph);
        const wng::GraphCommandResult second = failed_link(graph);

        assert(first.result == wng::Result::NotFound);
        assert(second.result == wng::Result::InvalidConnection);

        assert(transaction.append(first) == wng::Result::Ok);
        assert(transaction.append(second) == wng::Result::Ok);

        assert(transaction.result() == wng::Result::NotFound);
        assert(transaction.first_failed_command() != nullptr);
        assert(transaction.first_failed_command()->result == wng::Result::NotFound);
    }

    {
        // Verifies that committing a successful transaction stores one user-level
        // undo step in specialized command history without mutating the graph again.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        wng::GraphCommandTransaction transaction;
        const wng::GraphCommandResult created =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(transaction.append(created) == wng::Result::Ok);

        const wng::GraphTransactionResult committed =
            wng::commit_transaction(history, transaction);

        assert(committed.result == wng::Result::Ok);
        assert(committed.success());
        assert(history.can_undo());
        assert(!history.can_redo());
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
        assert(graph.find_node(created.record.node) != nullptr);
    }

    {
        // Verifies that committing to mixed graph-level history stores the same
        // command batch as one chronological graph history entry.
        wng::Graph graph;
        wng::GraphHistory history;
        wng::GraphCommandTransaction transaction;
        const wng::GraphCommandResult created =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(transaction.append(created) == wng::Result::Ok);

        const wng::GraphTransactionResult committed =
            wng::commit_transaction(history, transaction);

        assert(committed.result == wng::Result::Ok);
        assert(committed.success());
        assert(history.can_undo());
        assert(!history.can_redo());
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
        assert(graph.find_node(created.record.node) != nullptr);

        const wng::GraphHistoryOperationResult undone =
            wng::undo_last_graph_history(graph, history);
        assert(undone.result == wng::Result::Ok);
        assert(graph.find_node(created.record.node) == nullptr);
        assert(!history.can_undo());
        assert(history.can_redo());

        const wng::GraphHistoryOperationResult redone =
            wng::redo_last_graph_history(graph, history);
        assert(redone.result == wng::Result::Ok);
        assert(graph.find_node(created.record.node) != nullptr);
        assert(history.can_undo());
        assert(!history.can_redo());
    }

    {
        // Verifies that empty transactions are rejected at commit time so
        // specialized command history cannot accumulate confusing no-op undo entries.
        wng::GraphCommandHistory history;
        const wng::GraphCommandTransaction transaction;

        const wng::GraphTransactionResult committed =
            wng::commit_transaction(history, transaction);

        assert(committed.result == wng::Result::InvalidArgument);
        assert(!history.can_undo());
        assert(history.undo_count() == 0U);
    }

    {
        // Verifies that empty transactions are rejected by mixed graph-level
        // history for the same no-op history entry reason.
        wng::GraphHistory history;
        const wng::GraphCommandTransaction transaction;

        const wng::GraphTransactionResult committed =
            wng::commit_transaction(history, transaction);

        assert(committed.result == wng::Result::InvalidArgument);
        assert(!history.can_undo());
        assert(history.undo_count() == 0U);
    }

    {
        // Verifies that failed transactions cannot be committed. Failed records
        // remain diagnostic data, not safe undo/redo history units.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        wng::GraphCommandTransaction transaction;
        assert(transaction.append(failed_add_port(graph)) == wng::Result::Ok);

        const wng::GraphTransactionResult committed =
            wng::commit_transaction(history, transaction);

        assert(committed.result == wng::Result::InvalidArgument);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Verifies that failed transactions also cannot enter mixed graph-level
        // history because failed command records are not undoable effects.
        wng::Graph graph;
        wng::GraphHistory history;
        wng::GraphCommandTransaction transaction;
        assert(transaction.append(failed_add_port(graph)) == wng::Result::Ok);

        const wng::GraphTransactionResult committed =
            wng::commit_transaction(history, transaction);

        assert(committed.result == wng::Result::InvalidArgument);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Verifies rollback of a successful pending transaction. Rollback uses
        // the undo primitive but does not push anything into command history.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        wng::GraphCommandTransaction transaction;
        const wng::GraphCommandResult created =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(transaction.append(created) == wng::Result::Ok);

        const wng::GraphTransactionResult rolled_back =
            wng::rollback_transaction(graph, transaction);

        assert(rolled_back.result == wng::Result::Ok);
        assert(graph.find_node(created.record.node) == nullptr);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Verifies rollback after a mid-operation failure. Successful graph
        // effects are undone while failed records remain ignored for application.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        wng::GraphCommandTransaction transaction;
        const wng::GraphCommandResult created =
            wng::command_create_node(graph, make_node_desc("A"));
        const wng::GraphCommandResult failed = failed_add_port(graph);

        assert(transaction.append(created) == wng::Result::Ok);
        assert(transaction.append(failed) == wng::Result::Ok);
        assert(transaction.failed());

        const wng::GraphTransactionResult rolled_back =
            wng::rollback_transaction(graph, transaction);

        assert(rolled_back.result == wng::Result::Ok);
        assert(graph.find_node(created.record.node) == nullptr);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Verifies that rolling back an empty transaction is a no-op. This lets
        // callers abort empty editor operations without special-case handling.
        wng::Graph graph;
        const std::size_t node_count = graph.nodes().size();
        const std::size_t port_count = graph.ports().size();
        const std::size_t link_count = graph.links().size();
        const wng::GraphCommandTransaction transaction;

        const wng::GraphTransactionResult rolled_back =
            wng::rollback_transaction(graph, transaction);

        assert(rolled_back.result == wng::Result::Ok);
        assert(graph.nodes().size() == node_count);
        assert(graph.ports().size() == port_count);
        assert(graph.links().size() == link_count);
    }

    {
        // Verifies clear() resets both successful and failed pending records so a
        // transaction object can be reused safely after an aborted operation.
        wng::Graph graph;
        wng::GraphCommandTransaction transaction;
        assert(transaction.append(
            wng::command_create_node(graph, make_node_desc("A"))) == wng::Result::Ok);
        assert(transaction.append(failed_add_port(graph)) == wng::Result::Ok);

        transaction.clear();

        assert(transaction.empty());
        assert(!transaction.failed());
        assert(!transaction.committable());
        assert(transaction.record_count() == 0U);
        assert(transaction.result() == wng::Result::Ok);
        assert(transaction.first_failed_command() == nullptr);
    }

    {
        // Verifies append_batch preserves record order. Higher-level operations
        // can compose smaller batches without changing undo/redo semantics.
        wng::Graph graph;
        const wng::GraphCommandResult a =
            wng::command_create_node(graph, make_node_desc("A"));
        const wng::GraphCommandResult b =
            wng::command_create_node(graph, make_node_desc("B"));
        const wng::GraphCommandBatch batch = make_batch({ a.record, b.record });

        wng::GraphCommandTransaction transaction;
        assert(transaction.append_batch(batch) == wng::Result::Ok);

        assert(transaction.record_count() == 2U);
        assert(transaction.batch().records[0].node == a.record.node);
        assert(transaction.batch().records[1].node == b.record.node);
        assert(transaction.committable());
    }

    {
        // Verifies failed batches make the transaction failed while preserving
        // records for diagnostics and future UI error reporting.
        wng::Graph graph;
        wng::GraphCommandBatch batch = make_batch({ failed_add_port(graph).record });
        assert(batch.result == wng::Result::NotFound);

        wng::GraphCommandTransaction transaction;
        assert(transaction.append_batch(batch) == wng::Result::Ok);

        assert(transaction.failed());
        assert(!transaction.committable());
        assert(transaction.result() == wng::Result::NotFound);
    }

    {
        // Verifies append_batch rejects empty input batches so callers cannot
        // accidentally create an empty pending history step.
        const wng::GraphCommandBatch batch;
        wng::GraphCommandTransaction transaction;

        assert(transaction.append_batch(batch) == wng::Result::InvalidArgument);
        assert(transaction.empty());
    }

    {
        // Verifies transaction introspection is read-only. A transaction may be
        // queried freely by future editor code without changing graph state.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "Existing");
        const std::size_t node_count = graph.nodes().size();
        const std::size_t port_count = graph.ports().size();
        const std::size_t link_count = graph.links().size();
        const wng::GraphCommandTransaction transaction;

        (void)transaction.empty();
        (void)transaction.failed();
        (void)transaction.committable();
        (void)transaction.record_count();
        (void)transaction.batch();
        (void)transaction.first_failed_command();

        assert(graph.find_node(node) != nullptr);
        assert(graph.nodes().size() == node_count);
        assert(graph.ports().size() == port_count);
        assert(graph.links().size() == link_count);
    }

    return 0;
}
