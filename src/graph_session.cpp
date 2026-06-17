// Implements the WNG GraphSession product-facing working-state boundary.
// This file coordinates existing graph-core primitives without adding editor,
// rendering, WPL, persistence, or runtime evaluation behavior.

#include <wng/graph_session.hpp>

#include <wng/execution_plan.hpp>
#include <wng/graph_command.hpp>
#include <wng/graph_command_transaction.hpp>
#include <wng/graph_history.hpp>
#include <wng/graph_validation.hpp>
#include <wng/serialization.hpp>

namespace
{
    bool transaction_has_successful_command(const wng::GraphCommandTransaction& transaction)
    {
        for (const wng::GraphCommandRecord& record : transaction.batch().records) {
            if (record.result == wng::Result::Ok) {
                return true;
            }
        }

        return false;
    }
}

namespace wng
{
    bool GraphSessionCommandResult::success() const
    {
        return command.success() && history_result == Result::Ok;
    }

    Graph& GraphSession::graph()
    {
        return graph_;
    }

    const Graph& GraphSession::graph() const
    {
        return graph_;
    }

    GraphSchema& GraphSession::schema()
    {
        return schema_;
    }

    const GraphSchema& GraphSession::schema() const
    {
        return schema_;
    }

    GraphHistory& GraphSession::history()
    {
        return history_;
    }

    const GraphHistory& GraphSession::history() const
    {
        return history_;
    }

    std::uint64_t GraphSession::revision() const
    {
        return revision_;
    }

    bool GraphSession::dirty() const
    {
        return revision_ != saved_revision_;
    }

    void GraphSession::mark_saved()
    {
        saved_revision_ = revision_;
    }

    void GraphSession::mark_modified()
    {
        ++revision_;
    }

    void GraphSession::clear_history()
    {
        history_.clear();
    }

    ValidationReport GraphSession::validate() const
    {
        return validate_graph(graph_, schema_);
    }

    ValidationReport GraphSession::validate(const GraphSchemaValidationOptions& options) const
    {
        return validate_graph(graph_, schema_, options);
    }

    ExecutionPlan GraphSession::build_execution_plan(
        const ExecutionPlanRequest& request) const
    {
        return wng::build_execution_plan(graph_, schema_, request);
    }

    Result GraphSession::export_graph(GraphDto* out_graph) const
    {
        return wng::export_graph(graph_, out_graph);
    }

    Result GraphSession::import_graph(const GraphDto& graph_dto)
    {
        const Result result = wng::import_graph(graph_dto, &graph_);
        if (result != Result::Ok) {
            return result;
        }

        // Import replaces the graph baseline. Existing undo entries reference the
        // previous graph state, so keeping them would make session-level undo unsafe.
        history_.clear();
        mark_modified();
        return Result::Ok;
    }

    GraphSessionCommandResult GraphSession::create_node(const NodeDesc& desc)
    {
        return record_executed_command(wng::command_create_node(graph_, desc));
    }

    GraphSessionCommandResult GraphSession::create_schema_node(const NodeDesc& desc)
    {
        return record_executed_command(wng::command_create_node(graph_, schema_, desc));
    }

    GraphSessionCommandResult GraphSession::instantiate_node(const NodeDesc& desc)
    {
        return record_executed_command(wng::command_instantiate_node(graph_, schema_, desc));
    }

    GraphSessionCommandResult GraphSession::destroy_node(NodeId node)
    {
        return record_executed_command(wng::command_destroy_node(graph_, node));
    }

    GraphSessionCommandResult GraphSession::add_port(
        NodeId node,
        const PortDesc& desc)
    {
        return record_executed_command(wng::command_add_port(graph_, node, desc));
    }

    GraphSessionCommandResult GraphSession::add_schema_port(
        NodeId node,
        const PortDesc& desc)
    {
        return record_executed_command(wng::command_add_port(graph_, schema_, node, desc));
    }

    GraphSessionCommandResult GraphSession::remove_port(PortId port)
    {
        return record_executed_command(wng::command_remove_port(graph_, port));
    }

    GraphSessionCommandResult GraphSession::create_link(PortId from, PortId to)
    {
        return record_executed_command(wng::command_create_link(graph_, from, to));
    }

    GraphSessionCommandResult GraphSession::create_schema_link(PortId from, PortId to)
    {
        return record_executed_command(wng::command_create_link(graph_, schema_, from, to));
    }

    GraphSessionCommandResult GraphSession::destroy_link(LinkId link)
    {
        return record_executed_command(wng::command_destroy_link(graph_, link));
    }

    GraphTransactionResult GraphSession::commit_transaction(
        const GraphCommandTransaction& transaction)
    {
        const bool graph_already_changed = transaction_has_successful_command(transaction);
        const GraphTransactionResult result = wng::commit_transaction(history_, transaction);

        // Transactions contain command results from mutations that already ran
        // against this session graph. The revision must reflect those graph
        // effects even if the history commit is rejected or allocation fails.
        if (graph_already_changed) {
            mark_modified();
        }

        return result;
    }

    GraphTransactionResult GraphSession::rollback_transaction(
        const GraphCommandTransaction& transaction)
    {
        const bool rollback_can_change_graph = transaction_has_successful_command(transaction);
        const GraphTransactionResult result = wng::rollback_transaction(graph_, transaction);
        if (result.success() && rollback_can_change_graph) {
            mark_modified();
        }
        return result;
    }

    Result GraphSession::record_graph_command(const GraphCommandRecord& record)
    {
        const Result result = history_.record_graph_command(record);
        if (result == Result::Ok) {
            mark_modified();
        }
        return result;
    }

    Result GraphSession::record_graph_command_batch(const GraphCommandBatch& batch)
    {
        const Result result = history_.record_graph_command_batch(batch);
        if (result == Result::Ok) {
            mark_modified();
        }
        return result;
    }

    Result GraphSession::record_schema_migration_apply_command(
        const SchemaMigrationApplyCommandRecord& record)
    {
        const Result result = history_.record_schema_migration_apply_command(record);
        if (result == Result::Ok) {
            mark_modified();
        }
        return result;
    }

    GraphHistoryOperationResult GraphSession::undo()
    {
        const GraphHistoryOperationResult result = undo_last_graph_history(graph_, history_);
        if (result.success()) {
            mark_modified();
        }
        return result;
    }

    GraphHistoryOperationResult GraphSession::redo()
    {
        const GraphHistoryOperationResult result = redo_last_graph_history(graph_, history_);
        if (result.success()) {
            mark_modified();
        }
        return result;
    }

    GraphSessionCommandResult GraphSession::record_executed_command(
        const GraphCommandResult& command_result)
    {
        GraphSessionCommandResult session_result;
        session_result.command = command_result;

        if (!command_result.success()) {
            return session_result;
        }

        session_result.history_result = history_.record_graph_command(command_result.record);

        // Command helpers mutate the graph before returning a successful record.
        // Even if history recording fails afterward, the visible graph has changed
        // and the session must report a new dirty revision.
        mark_modified();
        return session_result;
    }
}
