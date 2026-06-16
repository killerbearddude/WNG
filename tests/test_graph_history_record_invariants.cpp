// Exercises GraphHistory record-rejection invariants.
// Rejected graph-command and schema-migration records must not disturb existing
// undo/redo stacks or mutate graph state.

#include <cassert>
#include <cstddef>

#include <wng/graph_history.hpp>
#include <wng/schema_migration_apply_command.hpp>

namespace
{
    wng::NodeDesc make_node_desc(const char* title)
    {
        wng::NodeDesc desc;
        desc.type = "history.node";
        desc.title = title;
        desc.position = wng::Vec2 { 1.0f, 2.0f };
        desc.size = wng::Vec2 { 100.0f, 50.0f };
        return desc;
    }

    wng::PortDesc make_port_desc()
    {
        wng::PortDesc desc;
        desc.kind = wng::PortKind::Input;
        desc.name = "in";
        desc.type = "number";
        return desc;
    }
}

int main()
{
    {
        // Invalid graph-command batches are diagnostics only. A failed record must
        // not clear an existing redo branch or change the graph state.
        wng::Graph graph;
        wng::GraphHistory history;

        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(create.result == wng::Result::Ok);
        assert(history.record_graph_command(create.record) == wng::Result::Ok);
        assert(wng::undo_last_graph_history(graph, history).success());
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);

        const std::size_t node_count_before_record = graph.nodes().size();
        const wng::GraphCommandResult failed =
            wng::command_add_port(
                graph,
                wng::NodeId { 999 },
                make_port_desc());
        assert(failed.result != wng::Result::Ok);

        wng::GraphCommandBatch invalid_batch;
        invalid_batch.result = wng::Result::Ok;
        invalid_batch.records.push_back(failed.record);

        assert(history.record_graph_command_batch(invalid_batch) ==
            wng::Result::InvalidArgument);
        assert(graph.nodes().size() == node_count_before_record);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);

        assert(wng::redo_last_graph_history(graph, history).success());
        assert(graph.find_node(create.record.node) != nullptr);
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
    }

    {
        // Invalid schema-migration records follow the same atomicity rule as graph
        // commands: rejecting the record must leave the current timeline intact.
        wng::Graph graph;
        wng::GraphHistory history;

        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(create.result == wng::Result::Ok);
        assert(history.record_graph_command(create.record) == wng::Result::Ok);
        assert(wng::undo_last_graph_history(graph, history).success());
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);

        const std::size_t node_count_before_record = graph.nodes().size();
        const wng::SchemaMigrationApplyCommandRecord invalid_record;

        assert(history.record_schema_migration_apply_command(invalid_record) ==
            wng::Result::InvalidArgument);
        assert(graph.nodes().size() == node_count_before_record);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);

        assert(wng::redo_last_graph_history(graph, history).success());
        assert(graph.find_node(create.record.node) != nullptr);
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
    }

    return 0;
}
