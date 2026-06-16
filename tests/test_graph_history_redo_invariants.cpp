// Exercises mixed GraphHistory redo failure invariants.
// GraphHistory must leave undo/redo stacks unchanged when delegated redo fails,
// matching the stack atomicity guarantees provided by specialized command history.

#include <cassert>
#include <cstddef>

#include <wng/graph_history.hpp>
#include <wng/graph_restore.hpp>

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
}

int main()
{
    {
        // Redo is staged before stack publication. If graph state has diverged and
        // the delegated redo cannot reapply the command record, the entry must stay
        // on the redo stack so callers can inspect or retry after repair.
        wng::Graph graph;
        wng::GraphHistory history;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(create.result == wng::Result::Ok);
        assert(history.record_graph_command(create.record) == wng::Result::Ok);

        assert(wng::undo_last_graph_history(graph, history).success());
        assert(graph.find_node(create.record.node) == nullptr);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);

        wng::GraphObjectSnapshot collision;
        collision.nodes = create.record.created_nodes;
        assert(wng::restore_graph_objects(graph, collision).success());
        const std::size_t node_count_before = graph.nodes().size();

        const wng::GraphHistoryOperationResult redo =
            wng::redo_last_graph_history(graph, history);

        assert(!redo.success());
        assert(graph.nodes().size() == node_count_before);
        assert(graph.find_node(create.record.node) != nullptr);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);
    }

    return 0;
}
