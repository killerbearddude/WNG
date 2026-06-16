// Exercises GraphHistory clear-state behavior.
// These checks verify that clearing history stacks affects only stack ownership;
// graph objects remain in their current state.

#include <cassert>
#include <cstddef>

#include <wng/graph_history.hpp>

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
        // clear_redo removes redo availability while preserving the remaining undo
        // entry and the current graph state.
        wng::Graph graph;
        wng::GraphHistory history;

        const wng::GraphCommandResult create_a =
            wng::command_create_node(graph, make_node_desc("A"));
        const wng::GraphCommandResult create_b =
            wng::command_create_node(graph, make_node_desc("B"));
        assert(create_a.result == wng::Result::Ok);
        assert(create_b.result == wng::Result::Ok);
        assert(history.record_graph_command(create_a.record) == wng::Result::Ok);
        assert(history.record_graph_command(create_b.record) == wng::Result::Ok);

        assert(wng::undo_last_graph_history(graph, history).success());
        assert(graph.find_node(create_a.record.node) != nullptr);
        assert(graph.find_node(create_b.record.node) == nullptr);
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 1U);

        const std::size_t node_count_before_clear = graph.nodes().size();
        history.clear_redo();

        assert(graph.nodes().size() == node_count_before_clear);
        assert(graph.find_node(create_a.record.node) != nullptr);
        assert(graph.find_node(create_b.record.node) == nullptr);
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
        assert(!wng::redo_last_graph_history(graph, history).success());

        assert(wng::undo_last_graph_history(graph, history).success());
        assert(graph.find_node(create_a.record.node) == nullptr);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);
    }

    {
        // clear removes all stack availability while leaving the graph exactly as
        // it was before the call.
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

        const std::size_t node_count_before_clear = graph.nodes().size();
        history.clear();

        assert(graph.nodes().size() == node_count_before_clear);
        assert(graph.find_node(create.record.node) == nullptr);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
        assert(!history.can_undo());
        assert(!history.can_redo());
        assert(!wng::undo_last_graph_history(graph, history).success());
        assert(!wng::redo_last_graph_history(graph, history).success());
    }

    return 0;
}
