// Provides WPL-free editor selection commands over GraphSession and
// GraphEditorState. This layer turns stable-ID selection into graph mutations
// without owning UI, hit testing, rendering, platform input, or application logic.

#pragma once

#include <cstddef>

#include <wng/graph_command_transaction.hpp>
#include <wng/graph_editor_state.hpp>
#include <wng/graph_session.hpp>
#include <wng/result.hpp>

namespace wng
{
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
