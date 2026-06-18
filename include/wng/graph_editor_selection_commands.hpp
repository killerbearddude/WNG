// Provides WPL-free editor selection commands over GraphSession and
// GraphEditorState. This layer turns stable-ID selection into graph mutations
// without owning UI, hit testing, rendering, platform input, or application logic.

#pragma once

#include <cstddef>

#include <wng/graph.hpp>
#include <wng/graph_command_transaction.hpp>
#include <wng/graph_editor_state.hpp>
#include <wng/graph_session.hpp>
#include <wng/result.hpp>

namespace wng
{
    // Read-only graph-context availability for selection-driven graph deletion.
    // This distinguishes live selectable graph objects from stale editor IDs
    // without mutating Graph, GraphSession, command history, or editor state.
    struct GraphEditorSelectionCommandAvailability {
        std::size_t selected_nodes = 0;
        std::size_t selected_ports = 0;
        std::size_t selected_links = 0;
        std::size_t live_nodes = 0;
        std::size_t live_ports = 0;
        std::size_t live_links = 0;
        std::size_t stale_nodes = 0;
        std::size_t stale_ports = 0;
        std::size_t stale_links = 0;

        std::size_t selected_count() const;
        std::size_t live_count() const;
        std::size_t stale_count() const;

        bool has_selection() const;
        bool has_live_selection() const;
        bool has_stale_selection() const;
        bool only_stale_selection() const;

        bool delete_selected_nodes() const;
        bool delete_selected_ports() const;
        bool delete_selected_links() const;
        bool delete_selected_graph_objects() const;
        bool any_delete_available() const;
    };

    GraphEditorSelectionCommandAvailability graph_editor_selection_command_availability(
        const Graph& graph,
        const GraphEditorState& editor_state);

    // Reports the result of one selection-driven delete operation. Successful
    // graph command records are grouped into one transaction and committed to the
    // session as one user operation when possible.
    struct GraphEditorSelectionCommandResult {
        Result result = Result::Ok;
        GraphTransactionResult commit_result;
        GraphCommandTransaction transaction;
        std::size_t deleted_links = 0;
        std::size_t deleted_ports = 0;
        std::size_t deleted_nodes = 0;

        bool success() const;
        std::size_t deleted_count() const;
    };

    // Deletes selected links through the session graph and clears stale editor
    // state through mutation summaries. Stale selected link IDs are deselected.
    GraphEditorSelectionCommandResult delete_selected_links(
        GraphSession& session,
        GraphEditorState& editor_state);

    // Deletes selected ports through the session graph and clears removed ports
    // plus dependent links from editor state. Stale selected port IDs are
    // deselected.
    GraphEditorSelectionCommandResult delete_selected_ports(
        GraphSession& session,
        GraphEditorState& editor_state);

    // Deletes selected nodes through the session graph and clears removed nodes,
    // ports, and links from editor state. Stale selected node IDs are deselected.
    GraphEditorSelectionCommandResult delete_selected_nodes(
        GraphSession& session,
        GraphEditorState& editor_state);

    // Deletes selected links, then ports, then nodes as one logical editor
    // operation. That command record order lets batch undo restore nodes first,
    // then ports, then links.
    GraphEditorSelectionCommandResult delete_selected_graph_objects(
        GraphSession& session,
        GraphEditorState& editor_state);
}
