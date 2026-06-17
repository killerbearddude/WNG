// Bridges graph-space hit-test results into WNG editor state.
// This module updates stable-ID editor hover state without owning UI, rendering,
// screen/canvas transforms, platform input, widgets, or WPL integration.

#pragma once

#include <wng/graph.hpp>
#include <wng/graph_editor_state.hpp>
#include <wng/graph_hit_testing.hpp>
#include <wng/graph_session.hpp>
#include <wng/math.hpp>
#include <wng/result.hpp>

namespace wng
{
    // Reports one editor hover update driven by graph-space hit testing.
    struct GraphEditorHitTestUpdateResult {
        Result result = Result::Ok;
        GraphHitTestResult hit;
        GraphEditorElement hovered;

        bool success() const;
    };

    // Converts one hit-test result into the editor element representation used by
    // GraphEditorState. A miss becomes GraphEditorElementKind::None.
    GraphEditorElement graph_editor_element_from_hit_test(
        const GraphHitTestResult& hit);

    // Applies a hit-test result to GraphEditorState hover state. Misses clear the
    // hovered element. Invalid IDs are reported by the underlying hover setters.
    Result apply_graph_editor_hover(
        GraphEditorState& editor_state,
        const GraphHitTestResult& hit);

    // Hit tests graph-space coordinates against Graph and updates editor hover.
    GraphEditorHitTestUpdateResult update_graph_editor_hover_from_hit_test(
        const Graph& graph,
        GraphEditorState& editor_state,
        Vec2 graph_position,
        const GraphHitTestOptions& options = GraphHitTestOptions {});

    // Session convenience overload for product-facing callers that keep graph data
    // behind GraphSession.
    GraphEditorHitTestUpdateResult update_graph_editor_hover_from_hit_test(
        const GraphSession& session,
        GraphEditorState& editor_state,
        Vec2 graph_position,
        const GraphHitTestOptions& options = GraphHitTestOptions {});
}
