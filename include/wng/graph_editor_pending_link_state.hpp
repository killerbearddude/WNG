// Provides WPL-free graph-space helpers for editor pending-link state.
// This module stores stable IDs only and owns no UI, rendering, screen/canvas
// transform, platform input, widgets, or WPL integration.

#pragma once

#include <wng/graph.hpp>
#include <wng/graph_editor_state.hpp>
#include <wng/graph_hit_testing.hpp>
#include <wng/graph_session.hpp>
#include <wng/math.hpp>
#include <wng/result.hpp>

namespace wng
{
    struct GraphEditorPendingLinkHitTestUpdateResult {
        Result result = Result::Ok;
        GraphHitTestResult hit;
        GraphEditorPendingLink pending_link;

        bool success() const
        {
            return result == Result::Ok;
        }
    };

    struct GraphEditorPendingLinkCompleteResult {
        GraphSessionCommandResult session;
        GraphEditorPendingLink pending_link;

        bool success() const
        {
            return session.success();
        }
    };

    inline bool graph_editor_pending_link_valid_port(PortId port)
    {
        return port.value != 0;
    }

    // Starts pending-link state from a port hit. Non-port hits clear pending-link
    // state. This does not create graph links.
    inline Result begin_graph_editor_pending_link_from_hit(
        GraphEditorState& editor_state,
        const GraphHitTestResult& hit)
    {
        if (hit.kind == GraphHitTestKind::None ||
            hit.kind == GraphHitTestKind::Node ||
            hit.kind == GraphHitTestKind::Link) {
            editor_state.clear_pending_link();
            return Result::Ok;
        }
        if (hit.kind != GraphHitTestKind::Port ||
            !graph_editor_pending_link_valid_port(hit.port)) {
            return Result::InvalidArgument;
        }

        return editor_state.begin_pending_link(hit.port);
    }

    // Updates pending-link target state from a port hit. Non-port hits clear only
    // the target. The active source port is not accepted as its own target.
    inline Result update_graph_editor_pending_link_target_from_hit(
        GraphEditorState& editor_state,
        const GraphHitTestResult& hit)
    {
        const GraphEditorPendingLink pending_link = editor_state.pending_link();
        if (hit.kind == GraphHitTestKind::None ||
            hit.kind == GraphHitTestKind::Node ||
            hit.kind == GraphHitTestKind::Link) {
            editor_state.clear_pending_link_target();
            return Result::Ok;
        }
        if (hit.kind != GraphHitTestKind::Port ||
            !graph_editor_pending_link_valid_port(hit.port)) {
            return Result::InvalidArgument;
        }
        if (!pending_link.active) {
            return Result::InvalidArgument;
        }
        if (hit.port == pending_link.from) {
            editor_state.clear_pending_link_target();
            return Result::Ok;
        }

        return editor_state.set_pending_link_target(hit.port);
    }

    inline GraphEditorPendingLinkHitTestUpdateResult graph_editor_pending_link_update_result(
        const GraphHitTestResult& hit,
        const GraphEditorState& editor_state,
        Result result)
    {
        GraphEditorPendingLinkHitTestUpdateResult update;
        update.result = result;
        update.hit = hit;
        update.pending_link = editor_state.pending_link();
        return update;
    }

    // Completes the active pending-link state through GraphSession's schema-aware
    // link command. Pending state is cleared after graph mutation succeeds; the
    // returned session result still reports history-recording failures separately.
    inline GraphEditorPendingLinkCompleteResult complete_graph_editor_pending_link(
        GraphSession& session,
        GraphEditorState& editor_state)
    {
        GraphEditorPendingLinkCompleteResult result;
        result.pending_link = editor_state.pending_link();
        if (!result.pending_link.active ||
            !graph_editor_pending_link_valid_port(result.pending_link.from) ||
            !graph_editor_pending_link_valid_port(result.pending_link.candidate_to)) {
            result.session.command.result = Result::InvalidArgument;
            result.session.command.record.kind = GraphCommandKind::SchemaCreateLink;
            result.session.command.record.result = Result::InvalidArgument;
            return result;
        }

        result.session = session.create_schema_link(
            result.pending_link.from,
            result.pending_link.candidate_to);
        if (result.session.command.success()) {
            editor_state.clear_pending_link();
        }
        return result;
    }

    inline GraphEditorPendingLinkHitTestUpdateResult begin_graph_editor_pending_link_from_hit_test(
        const Graph& graph,
        GraphEditorState& editor_state,
        Vec2 graph_position,
        const GraphHitTestOptions& options = GraphHitTestOptions {})
    {
        const GraphHitTestResult hit = hit_test_graph(graph, graph_position, options);
        const Result result = begin_graph_editor_pending_link_from_hit(editor_state, hit);
        return graph_editor_pending_link_update_result(hit, editor_state, result);
    }

    inline GraphEditorPendingLinkHitTestUpdateResult begin_graph_editor_pending_link_from_hit_test(
        const GraphSession& session,
        GraphEditorState& editor_state,
        Vec2 graph_position,
        const GraphHitTestOptions& options = GraphHitTestOptions {})
    {
        return begin_graph_editor_pending_link_from_hit_test(
            session.graph(),
            editor_state,
            graph_position,
            options);
    }

    inline GraphEditorPendingLinkHitTestUpdateResult update_graph_editor_pending_link_target_from_hit_test(
        const Graph& graph,
        GraphEditorState& editor_state,
        Vec2 graph_position,
        const GraphHitTestOptions& options = GraphHitTestOptions {})
    {
        const GraphHitTestResult hit = hit_test_graph(graph, graph_position, options);
        const Result result = update_graph_editor_pending_link_target_from_hit(editor_state, hit);
        return graph_editor_pending_link_update_result(hit, editor_state, result);
    }

    inline GraphEditorPendingLinkHitTestUpdateResult update_graph_editor_pending_link_target_from_hit_test(
        const GraphSession& session,
        GraphEditorState& editor_state,
        Vec2 graph_position,
        const GraphHitTestOptions& options = GraphHitTestOptions {})
    {
        return update_graph_editor_pending_link_target_from_hit_test(
            session.graph(),
            editor_state,
            graph_position,
            options);
    }
}
