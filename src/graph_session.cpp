// Implements the WNG GraphSession product-facing working-state boundary.
// This file coordinates existing graph-core primitives without adding editor,
// rendering, WPL, persistence, or runtime evaluation behavior.

#include <wng/graph_session.hpp>

#include <wng/execution_plan.hpp>
#include <wng/graph_history.hpp>
#include <wng/graph_validation.hpp>
#include <wng/serialization.hpp>

namespace wng
{
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
}
