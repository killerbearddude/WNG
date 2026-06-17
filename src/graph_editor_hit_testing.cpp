// Implements WPL-free graph-space hit-test integration for editor state.
// This file intentionally maps existing graph hit-test results into stable-ID
// editor state only; it does not handle input events, rendering, widgets,
// transforms, mouse/keyboard routing, or WPL integration.

#include <wng/graph_editor_hit_testing.hpp>

namespace
{
    bool is_selected(
        const wng::GraphEditorState& editor_state,
        const wng::GraphEditorElement& element)
    {
        switch (element.kind) {
        case wng::GraphEditorElementKind::None:
            return false;
        case wng::GraphEditorElementKind::Node:
            return editor_state.node_selected(element.node);
        case wng::GraphEditorElementKind::Port:
            return editor_state.port_selected(element.port);
        case wng::GraphEditorElementKind::Link:
            return editor_state.link_selected(element.link);
        }

        return false;
    }

    bool selection_is_only(
        const wng::GraphEditorState& editor_state,
        const wng::GraphEditorElement& element)
    {
        switch (element.kind) {
        case wng::GraphEditorElementKind::None:
            return editor_state.selected_nodes().empty() &&
                   editor_state.selected_ports().empty() &&
                   editor_state.selected_links().empty();
        case wng::GraphEditorElementKind::Node:
            return editor_state.selected_nodes().size() == 1 &&
                   editor_state.selected_nodes().front() == element.node &&
                   editor_state.selected_ports().empty() &&
                   editor_state.selected_links().empty();
        case wng::GraphEditorElementKind::Port:
            return editor_state.selected_nodes().empty() &&
                   editor_state.selected_ports().size() == 1 &&
                   editor_state.selected_ports().front() == element.port &&
                   editor_state.selected_links().empty();
        case wng::GraphEditorElementKind::Link:
            return editor_state.selected_nodes().empty() &&
                   editor_state.selected_ports().empty() &&
                   editor_state.selected_links().size() == 1 &&
                   editor_state.selected_links().front() == element.link;
        }

        return false;
    }

    void deselect_element(
        wng::GraphEditorState& editor_state,
        const wng::GraphEditorElement& element)
    {
        switch (element.kind) {
        case wng::GraphEditorElementKind::None:
            return;
        case wng::GraphEditorElementKind::Node:
            editor_state.deselect_node(element.node);
            return;
        case wng::GraphEditorElementKind::Port:
            editor_state.deselect_port(element.port);
            return;
        case wng::GraphEditorElementKind::Link:
            editor_state.deselect_link(element.link);
            return;
        }
    }

    wng::Result select_element(
        wng::GraphEditorState& editor_state,
        const wng::GraphEditorElement& element)
    {
        if (!element.valid()) {
            return wng::Result::InvalidArgument;
        }
        if (is_selected(editor_state, element)) {
            return wng::Result::Ok;
        }

        switch (element.kind) {
        case wng::GraphEditorElementKind::None:
            return wng::Result::InvalidArgument;
        case wng::GraphEditorElementKind::Node:
            return editor_state.select_node(element.node);
        case wng::GraphEditorElementKind::Port:
            return editor_state.select_port(element.port);
        case wng::GraphEditorElementKind::Link:
            return editor_state.select_link(element.link);
        }

        return wng::Result::InvalidArgument;
    }
}

namespace wng
{
    bool GraphEditorHitTestUpdateResult::success() const
    {
        return result == Result::Ok;
    }

    bool GraphEditorSelectionUpdateResult::success() const
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

    Result apply_graph_editor_selection(
        GraphEditorState& editor_state,
        const GraphHitTestResult& hit,
        GraphEditorSelectionMode mode)
    {
        const GraphEditorElement element = graph_editor_element_from_hit_test(hit);
        if (element.kind == GraphEditorElementKind::None) {
            if (mode == GraphEditorSelectionMode::Replace) {
                editor_state.clear_selection();
            }
            return Result::Ok;
        }
        if (!element.valid()) {
            return Result::InvalidArgument;
        }

        switch (mode) {
        case GraphEditorSelectionMode::Replace:
            if (selection_is_only(editor_state, element)) {
                return Result::Ok;
            }
            editor_state.clear_selection();
            return select_element(editor_state, element);
        case GraphEditorSelectionMode::Add:
            return select_element(editor_state, element);
        case GraphEditorSelectionMode::Toggle:
            if (is_selected(editor_state, element)) {
                deselect_element(editor_state, element);
                return Result::Ok;
            }
            return select_element(editor_state, element);
        }

        return Result::InvalidArgument;
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

    GraphEditorSelectionUpdateResult update_graph_editor_selection_from_hit_test(
        const Graph& graph,
        GraphEditorState& editor_state,
        Vec2 graph_position,
        GraphEditorSelectionMode mode,
        const GraphHitTestOptions& options)
    {
        GraphEditorSelectionUpdateResult result;
        result.mode = mode;
        result.hit = hit_test_graph(graph, graph_position, options);
        result.target = graph_editor_element_from_hit_test(result.hit);
        result.result = apply_graph_editor_selection(editor_state, result.hit, mode);
        return result;
    }

    GraphEditorSelectionUpdateResult update_graph_editor_selection_from_hit_test(
        const GraphSession& session,
        GraphEditorState& editor_state,
        Vec2 graph_position,
        GraphEditorSelectionMode mode,
        const GraphHitTestOptions& options)
    {
        return update_graph_editor_selection_from_hit_test(
            session.graph(),
            editor_state,
            graph_position,
            mode,
            options);
    }
}
