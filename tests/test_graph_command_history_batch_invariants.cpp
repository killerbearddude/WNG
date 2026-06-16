// Exercises batch recordability invariants for GraphCommandHistory.
// History must reject mixed-result batches even when the batch-level result says
// Ok, and rejection must not disturb existing undo or redo stacks.

#include <cassert>

#include <wng/graph_command_history.hpp>

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
        // A batch-level Ok result is not sufficient for history safety. Every
        // contained command record must also represent a successful graph effect.
        wng::Graph graph;
        wng::GraphCommandHistory history;

        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(create.result == wng::Result::Ok);
        assert(history.record(create.record) == wng::Result::Ok);
        assert(wng::undo_last(graph, history).success());
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);

        const wng::GraphCommandResult failed =
            wng::command_add_port(
                graph,
                wng::NodeId { 999 },
                make_port_desc());
        assert(failed.result != wng::Result::Ok);

        wng::GraphCommandBatch mixed_batch;
        mixed_batch.result = wng::Result::Ok;
        mixed_batch.records.push_back(create.record);
        mixed_batch.records.push_back(failed.record);

        assert(history.record_batch(mixed_batch) == wng::Result::InvalidArgument);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);

        const wng::GraphHistoryResult redo = wng::redo_last(graph, history);
        assert(redo.success());
        assert(graph.find_node(create.record.node) != nullptr);
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
    }

    return 0;
}
