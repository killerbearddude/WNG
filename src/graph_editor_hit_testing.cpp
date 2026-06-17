// Implements WPL-free graph-space hit-test integration for editor hover state.
// This file intentionally maps existing graph hit-test results into stable-ID
// editor state only; it does not handle input events, rendering, widgets,
// transforms, selection policy, or WPL integration.

#include <wng/graph_editor_hit_testing.hpp>

namespace wng
{
    bool GraphEditorHitTestUpdateResult::success() const
    {
        return result == Result::Ok;
    }

    GraphEditorElement graph_editor_element_from_hit_test(
        const GraphHitTestResult& hit)
    {
        GraphEditorElement element;
        switch (hit.kind) {
        case GraphHitTestKind::None:
            return element;
        case GraphHitTestKind::Node:
            element.kind = GraphEditorElementKind::Node;
            element.node = hit.node;
            return element;
        case GraphHitTestKind::Port:
            element.kind = GraphEditorElementKind::Port;
            element.port = hit.port;
            return element;
        case GraphHitTestKind::Link:
            element.kind = GraphEditorElementKind::Link;
            element.link = hit.link;
            return element;
        }

        return element;
    }

    Result apply_graph_editor_hover(
        GraphEditorState& editor_state,
        const GraphHitTestResult& hit)
    {
        switch (hit.kind) {
        case GraphHitTestKind::None:
            editor_state.clear_hovered();
            return Result::Ok;
        case GraphHitTestKind::Node:
            return editor_state.set_hovered_node(hit.node);
        case GraphHitTestKind::Port:
            return editor_state.set_hovered_port(hit.port);
        case GraphHitTestKind::Link:
            return editor_state.set_hovered_link(hit.link);
        }

        editor_state.clear_hovered();
        return Result::Ok;
    }

    GraphEditorHitTestUpdateResult update_graph_editor_hover_from_hit_test(
        const Graph& graph,
        GraphEditorState& editor_state,
        Vec2 graph_position,
        const GraphHitTestOptions& options)
    {
        GraphEditorHitTestUpdateResult result;
        result.hit = hit_test_graph(graph, graph_position, options);
        result.hovered = graph_editor_element_from_hit_test(result.hit);
        result.result = apply_graph_editor_hover(editor_state, result.hit);
        if (result.result != Result::Ok) {
            result.hovered = editor_state.hovered();
        }
        return result;
    }

    GraphEditorHitTestUpdateResult update_graph_editor_hover_from_hit_test(
        const GraphSession& session,
        GraphEditorState& editor_state,
        Vec2 graph_position,
        const GraphHitTestOptions& options)
    {
        return update_graph_editor_hover_from_hit_test(
            session.graph(),
            editor_state,
            graph_position,
            options);
    }
}
