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
    // Identifies graph-context selection commands that operate over currently
    // selected graph objects.
    enum class GraphEditorSelectionCommandId {
        DeleteSelectedNodes,
        DeleteSelectedPorts,
        DeleteSelectedLinks,
        DeleteSelectedGraphObjects
    };

    // Explains why a graph-context selection command is unavailable. These are
    // state reasons only; user-facing labels and menu/tooltips belong above WNG.
    enum class GraphEditorSelectionCommandUnavailableReason {
        None,
        NoSelection,
        OnlyStaleSelection,
        NoLiveNodes,
        NoLivePorts,
        NoLiveLinks,
        InvalidCommand
    };

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

        std::size_t selected_count() const
        {
            return selected_nodes + selected_ports + selected_links;
        }

        std::size_t live_count() const
        {
            return live_nodes + live_ports + live_links;
        }

        std::size_t stale_count() const
        {
            return stale_nodes + stale_ports + stale_links;
        }

        bool has_selection() const
        {
            return selected_count() > 0U;
        }

        bool has_live_selection() const
        {
            return live_count() > 0U;
        }

        bool has_stale_selection() const
        {
            return stale_count() > 0U;
        }

        bool only_stale_selection() const
        {
            return has_selection() && !has_live_selection();
        }

        bool delete_selected_nodes() const
        {
            return live_nodes > 0U;
        }

        bool delete_selected_ports() const
        {
            return live_ports > 0U;
        }

        bool delete_selected_links() const
        {
            return live_links > 0U;
        }

        bool delete_selected_graph_objects() const
        {
            return has_live_selection();
        }

        bool any_delete_available() const
        {
            return delete_selected_nodes() ||
                delete_selected_ports() ||
                delete_selected_links() ||
                delete_selected_graph_objects();
        }
    };

    // Read-only status for one graph-context selection command. This preserves
    // the count-based availability snapshot while adding deterministic reasons
    // for disabled commands.
    struct GraphEditorSelectionCommandStatus {
        bool available = false;
        GraphEditorSelectionCommandUnavailableReason unavailable_reason =
            GraphEditorSelectionCommandUnavailableReason::None;
    };

    inline GraphEditorSelectionCommandAvailability graph_editor_selection_command_availability(
        const Graph& graph,
        const GraphEditorState& editor_state)
    {
        GraphEditorSelectionCommandAvailability availability;

        for (NodeId node : editor_state.selected_nodes()) {
            ++availability.selected_nodes;
            if (graph.find_node(node) != nullptr) {
                ++availability.live_nodes;
            } else {
                ++availability.stale_nodes;
            }
        }

        for (PortId port : editor_state.selected_ports()) {
            ++availability.selected_ports;
            if (graph.find_port(port) != nullptr) {
                ++availability.live_ports;
            } else {
                ++availability.stale_ports;
            }
        }

        for (LinkId link : editor_state.selected_links()) {
            ++availability.selected_links;
            if (graph.find_link(link) != nullptr) {
                ++availability.live_links;
            } else {
                ++availability.stale_links;
            }
        }

        return availability;
    }

    inline GraphEditorSelectionCommandStatus graph_editor_selection_command_status(
        const GraphEditorSelectionCommandAvailability& availability,
        GraphEditorSelectionCommandId command)
    {
        GraphEditorSelectionCommandStatus status;
        switch (command) {
        case GraphEditorSelectionCommandId::DeleteSelectedNodes:
            status.available = availability.delete_selected_nodes();
            if (status.available) {
                return status;
            }
            status.unavailable_reason = !availability.has_selection() ?
                GraphEditorSelectionCommandUnavailableReason::NoSelection :
                (availability.only_stale_selection() ?
                    GraphEditorSelectionCommandUnavailableReason::OnlyStaleSelection :
                    GraphEditorSelectionCommandUnavailableReason::NoLiveNodes);
            return status;
        case GraphEditorSelectionCommandId::DeleteSelectedPorts:
            status.available = availability.delete_selected_ports();
            if (status.available) {
                return status;
            }
            status.unavailable_reason = !availability.has_selection() ?
                GraphEditorSelectionCommandUnavailableReason::NoSelection :
                (availability.only_stale_selection() ?
                    GraphEditorSelectionCommandUnavailableReason::OnlyStaleSelection :
                    GraphEditorSelectionCommandUnavailableReason::NoLivePorts);
            return status;
        case GraphEditorSelectionCommandId::DeleteSelectedLinks:
            status.available = availability.delete_selected_links();
            if (status.available) {
                return status;
            }
            status.unavailable_reason = !availability.has_selection() ?
                GraphEditorSelectionCommandUnavailableReason::NoSelection :
                (availability.only_stale_selection() ?
                    GraphEditorSelectionCommandUnavailableReason::OnlyStaleSelection :
                    GraphEditorSelectionCommandUnavailableReason::NoLiveLinks);
            return status;
        case GraphEditorSelectionCommandId::DeleteSelectedGraphObjects:
            status.available = availability.delete_selected_graph_objects();
            if (status.available) {
                return status;
            }
            status.unavailable_reason = !availability.has_selection() ?
                GraphEditorSelectionCommandUnavailableReason::NoSelection :
                GraphEditorSelectionCommandUnavailableReason::OnlyStaleSelection;
            return status;
        }

        status.available = false;
        status.unavailable_reason =
            GraphEditorSelectionCommandUnavailableReason::InvalidCommand;
        return status;
    }

    inline GraphEditorSelectionCommandStatus graph_editor_selection_command_status(
        const Graph& graph,
        const GraphEditorState& editor_state,
        GraphEditorSelectionCommandId command)
    {
        return graph_editor_selection_command_status(
            graph_editor_selection_command_availability(graph, editor_state),
            command);
    }

    inline bool graph_editor_selection_command_available(
        const GraphEditorSelectionCommandAvailability& availability,
        GraphEditorSelectionCommandId command)
    {
        return graph_editor_selection_command_status(availability, command).available;
    }

    inline bool graph_editor_selection_command_available(
        const Graph& graph,
        const GraphEditorState& editor_state,
        GraphEditorSelectionCommandId command)
    {
        return graph_editor_selection_command_status(
            graph,
            editor_state,
            command).available;
    }

    // Reports the result of one selection-driven delete operation. Successful
    // graph command records are grouped into one transaction and committed to the
    // session as one user operation when possible.
    struct GraphEditorSelectionCommandResult {
        Result result = Result::Ok;
        GraphTransactionResult commit_result;
        GraphCommandTransaction transaction;
        GraphEditorSelectionCommandAvailability before;
        GraphEditorSelectionCommandAvailability after;
        std::size_t deleted_links = 0;
        std::size_t deleted_ports = 0;
        std::size_t deleted_nodes = 0;

        bool success() const;
        std::size_t deleted_count() const;
    };

    // Status transition for one graph-context selection command across a
    // completed selection command result. This is read-only introspection over
    // captured before/after availability snapshots and does not re-run commands.
    struct GraphEditorSelectionCommandResultStatus {
        GraphEditorSelectionCommandStatus before;
        GraphEditorSelectionCommandStatus after;

        bool availability_changed() const
        {
            return before.available != after.available;
        }

        bool became_available() const
        {
            return !before.available && after.available;
        }

        bool became_unavailable() const
        {
            return before.available && !after.available;
        }
    };

    inline GraphEditorSelectionCommandStatus graph_editor_selection_command_result_before_status(
        const GraphEditorSelectionCommandResult& result,
        GraphEditorSelectionCommandId command)
    {
        return graph_editor_selection_command_status(result.before, command);
    }

    inline GraphEditorSelectionCommandStatus graph_editor_selection_command_result_after_status(
        const GraphEditorSelectionCommandResult& result,
        GraphEditorSelectionCommandId command)
    {
        return graph_editor_selection_command_status(result.after, command);
    }

    inline GraphEditorSelectionCommandResultStatus graph_editor_selection_command_result_status(
        const GraphEditorSelectionCommandResult& result,
        GraphEditorSelectionCommandId command)
    {
        GraphEditorSelectionCommandResultStatus status;
        status.before = graph_editor_selection_command_result_before_status(
            result,
            command);
        status.after = graph_editor_selection_command_result_after_status(
            result,
            command);
        return status;
    }

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
