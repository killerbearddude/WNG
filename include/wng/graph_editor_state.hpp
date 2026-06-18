// Provides WNG editor-facing graph state without UI, rendering, hit testing,
// screen coordinates, WPL integration, or platform input ownership.

#pragma once

#include <cstddef>
#include <vector>

#include <wng/ids.hpp>
#include <wng/mutation_summary.hpp>
#include <wng/result.hpp>

namespace wng
{
    // Identifies the kind of graph object currently referenced by editor state.
    // The None value is used for cleared hover/pending-object state.
    enum class GraphEditorElementKind {
        None,
        Node,
        Port,
        Link
    };

    // Identifies editor-state-only commands that require no graph object operand.
    // Commands that require IDs stay as explicit typed helpers to avoid weakly
    // typed payloads in the graph core API.
    enum class GraphEditorCommandId {
        ClearSelection,
        ClearHover,
        CancelPendingLink
    };

    // Explains why a no-operand editor-state command is unavailable. The values
    // describe graph-core state only and do not encode UI presentation policy.
    enum class GraphEditorCommandUnavailableReason {
        None,
        NoSelection,
        NoHover,
        NoActivePendingLink,
        InvalidCommand
    };

    // Stable-ID reference to one graph object. Only the field matching kind is
    // meaningful; the other IDs remain invalid sentinel values.
    struct GraphEditorElement {
        GraphEditorElementKind kind = GraphEditorElementKind::None;
        NodeId node {};
        PortId port {};
        LinkId link {};

        bool valid() const;
    };

    // Compact read-only summary of editor selection state. This lets higher
    // layers decide command availability without inspecting the selection vectors
    // directly or depending on UI/editor widgets.
    struct GraphEditorSelectionSummary {
        std::size_t node_count = 0;
        std::size_t port_count = 0;
        std::size_t link_count = 0;

        std::size_t total_count() const;
        bool empty() const;
        bool single() const;
    };

    // Read-only command availability flags derived from editor state. This does
    // not execute commands, mutate graphs, inspect schema, or own UI behavior.
    struct GraphEditorCommandAvailability {
        bool clear_selection = false;
        bool remove_selection = false;
        bool clear_hover = false;
        bool cancel_pending_link = false;
        bool complete_pending_link = false;

        bool any_available() const
        {
            return clear_selection ||
                remove_selection ||
                clear_hover ||
                cancel_pending_link ||
                complete_pending_link;
        }
    };

    // Read-only command status for a no-operand editor-state command. Availability
    // remains a simple boolean, while unavailable_reason gives higher layers a
    // deterministic explanation without involving UI widgets or presentation text.
    struct GraphEditorCommandStatus {
        bool available = false;
        GraphEditorCommandUnavailableReason unavailable_reason =
            GraphEditorCommandUnavailableReason::None;
    };

    // Reports the outcome of an editor-state-only command. These commands do not
    // touch graph storage, command history, schema policy, or GraphSession.
    struct GraphEditorStateCommandResult {
        Result result = Result::Ok;
        bool changed = false;
        GraphEditorCommandAvailability before;
        GraphEditorCommandAvailability after;

        bool success() const
        {
            return result == Result::Ok;
        }
    };

    // Status transition for one command ID across a completed editor-state command
    // result. This is read-only introspection over the captured before/after
    // snapshots and does not re-run or mutate editor state.
    struct GraphEditorCommandResultStatus {
        GraphEditorCommandStatus before;
        GraphEditorCommandStatus after;

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

    // Captures a non-rendering pending link interaction. WNG core stores only
    // stable port IDs here; screen-space mouse positions and hit testing belong
    // to higher layers.
    struct GraphEditorPendingLink {
        bool active = false;
        PortId from {};
        PortId candidate_to {};
    };

    // Holds editor-facing graph state that can sit beside GraphSession. This
    // class intentionally owns no widget, renderer, WPL object, view transform,
    // hit-test policy, command execution, or application-specific behavior.
    class GraphEditorState {
    public:
        const std::vector<NodeId>& selected_nodes() const;
        const std::vector<PortId>& selected_ports() const;
        const std::vector<LinkId>& selected_links() const;

        GraphEditorElement hovered() const;
        GraphEditorPendingLink pending_link() const;

        Result select_node(NodeId node);
        Result select_port(PortId port);
        Result select_link(LinkId link);

        void deselect_node(NodeId node);
        void deselect_port(PortId port);
        void deselect_link(LinkId link);
        void clear_selection();

        bool node_selected(NodeId node) const;
        bool port_selected(PortId port) const;
        bool link_selected(LinkId link) const;

        Result set_hovered_node(NodeId node);
        Result set_hovered_port(PortId port);
        Result set_hovered_link(LinkId link);
        void clear_hovered();

        Result begin_pending_link(PortId from);
        Result set_pending_link_target(PortId candidate_to);
        void clear_pending_link_target();
        void clear_pending_link();

        // Removes editor references to graph objects that were destroyed by a
        // graph mutation. This keeps selection, hover, and pending-link state from
        // retaining broken stable IDs after destructive graph operations.
        void apply_mutation_summary(const GraphMutationSummary& summary);

    private:
        std::vector<NodeId> selected_nodes_;
        std::vector<PortId> selected_ports_;
        std::vector<LinkId> selected_links_;
        GraphEditorElement hovered_;
        GraphEditorPendingLink pending_link_;
    };

    GraphEditorSelectionSummary graph_editor_selection_summary(
        const GraphEditorState& editor_state);

    bool graph_editor_has_selection(const GraphEditorState& editor_state);
    bool graph_editor_has_hover(const GraphEditorState& editor_state);
    bool graph_editor_has_active_pending_link(const GraphEditorState& editor_state);
    bool graph_editor_has_pending_link_candidate(const GraphEditorState& editor_state);
    bool graph_editor_can_complete_pending_link(const GraphEditorState& editor_state);

    GraphEditorElement graph_editor_single_selected_element(
        const GraphEditorState& editor_state);

    inline GraphEditorCommandAvailability graph_editor_command_availability(
        const GraphEditorState& editor_state)
    {
        GraphEditorCommandAvailability availability;
        availability.clear_selection = graph_editor_has_selection(editor_state);
        availability.remove_selection = availability.clear_selection;
        availability.clear_hover = graph_editor_has_hover(editor_state);
        availability.cancel_pending_link = graph_editor_has_active_pending_link(editor_state);
        availability.complete_pending_link = graph_editor_can_complete_pending_link(editor_state);
        return availability;
    }

    inline GraphEditorCommandStatus graph_editor_command_status(
        const GraphEditorCommandAvailability& availability,
        GraphEditorCommandId command)
    {
        GraphEditorCommandStatus status;
        switch (command) {
        case GraphEditorCommandId::ClearSelection:
            status.available = availability.clear_selection;
            status.unavailable_reason = status.available ?
                GraphEditorCommandUnavailableReason::None :
                GraphEditorCommandUnavailableReason::NoSelection;
            return status;
        case GraphEditorCommandId::ClearHover:
            status.available = availability.clear_hover;
            status.unavailable_reason = status.available ?
                GraphEditorCommandUnavailableReason::None :
                GraphEditorCommandUnavailableReason::NoHover;
            return status;
        case GraphEditorCommandId::CancelPendingLink:
            status.available = availability.cancel_pending_link;
            status.unavailable_reason = status.available ?
                GraphEditorCommandUnavailableReason::None :
                GraphEditorCommandUnavailableReason::NoActivePendingLink;
            return status;
        }

        status.available = false;
        status.unavailable_reason = GraphEditorCommandUnavailableReason::InvalidCommand;
        return status;
    }

    inline GraphEditorCommandStatus graph_editor_command_status(
        const GraphEditorState& editor_state,
        GraphEditorCommandId command)
    {
        return graph_editor_command_status(
            graph_editor_command_availability(editor_state),
            command);
    }

    inline bool graph_editor_command_available(
        const GraphEditorCommandAvailability& availability,
        GraphEditorCommandId command)
    {
        return graph_editor_command_status(availability, command).available;
    }

    inline bool graph_editor_command_available(
        const GraphEditorState& editor_state,
        GraphEditorCommandId command)
    {
        return graph_editor_command_status(editor_state, command).available;
    }

    inline GraphEditorCommandStatus graph_editor_command_result_before_status(
        const GraphEditorStateCommandResult& result,
        GraphEditorCommandId command)
    {
        return graph_editor_command_status(result.before, command);
    }

    inline GraphEditorCommandStatus graph_editor_command_result_after_status(
        const GraphEditorStateCommandResult& result,
        GraphEditorCommandId command)
    {
        return graph_editor_command_status(result.after, command);
    }

    inline GraphEditorCommandResultStatus graph_editor_command_result_status(
        const GraphEditorStateCommandResult& result,
        GraphEditorCommandId command)
    {
        GraphEditorCommandResultStatus status;
        status.before = graph_editor_command_result_before_status(result, command);
        status.after = graph_editor_command_result_after_status(result, command);
        return status;
    }

    inline GraphEditorStateCommandResult select_graph_editor_node(
        GraphEditorState& editor_state,
        NodeId node)
    {
        GraphEditorStateCommandResult result;
        result.before = graph_editor_command_availability(editor_state);
        result.result = editor_state.select_node(node);
        result.changed = result.success();
        result.after = graph_editor_command_availability(editor_state);
        return result;
    }

    inline GraphEditorStateCommandResult select_graph_editor_port(
        GraphEditorState& editor_state,
        PortId port)
    {
        GraphEditorStateCommandResult result;
        result.before = graph_editor_command_availability(editor_state);
        result.result = editor_state.select_port(port);
        result.changed = result.success();
        result.after = graph_editor_command_availability(editor_state);
        return result;
    }

    inline GraphEditorStateCommandResult select_graph_editor_link(
        GraphEditorState& editor_state,
        LinkId link)
    {
        GraphEditorStateCommandResult result;
        result.before = graph_editor_command_availability(editor_state);
        result.result = editor_state.select_link(link);
        result.changed = result.success();
        result.after = graph_editor_command_availability(editor_state);
        return result;
    }

    inline GraphEditorStateCommandResult set_graph_editor_hovered_node(
        GraphEditorState& editor_state,
        NodeId node)
    {
        GraphEditorStateCommandResult result;
        result.before = graph_editor_command_availability(editor_state);
        const GraphEditorElement previous = editor_state.hovered();
        result.result = editor_state.set_hovered_node(node);
        result.changed = result.success() &&
            (previous.kind != GraphEditorElementKind::Node || previous.node != node);
        result.after = graph_editor_command_availability(editor_state);
        return result;
    }

    inline GraphEditorStateCommandResult set_graph_editor_hovered_port(
        GraphEditorState& editor_state,
        PortId port)
    {
        GraphEditorStateCommandResult result;
        result.before = graph_editor_command_availability(editor_state);
        const GraphEditorElement previous = editor_state.hovered();
        result.result = editor_state.set_hovered_port(port);
        result.changed = result.success() &&
            (previous.kind != GraphEditorElementKind::Port || previous.port != port);
        result.after = graph_editor_command_availability(editor_state);
        return result;
    }

    inline GraphEditorStateCommandResult set_graph_editor_hovered_link(
        GraphEditorState& editor_state,
        LinkId link)
    {
        GraphEditorStateCommandResult result;
        result.before = graph_editor_command_availability(editor_state);
        const GraphEditorElement previous = editor_state.hovered();
        result.result = editor_state.set_hovered_link(link);
        result.changed = result.success() &&
            (previous.kind != GraphEditorElementKind::Link || previous.link != link);
        result.after = graph_editor_command_availability(editor_state);
        return result;
    }

    inline GraphEditorStateCommandResult begin_graph_editor_pending_link(
        GraphEditorState& editor_state,
        PortId from)
    {
        GraphEditorStateCommandResult result;
        result.before = graph_editor_command_availability(editor_state);
        const GraphEditorPendingLink previous = editor_state.pending_link();
        result.result = editor_state.begin_pending_link(from);
        result.changed = result.success() &&
            (!previous.active || previous.from != from || previous.candidate_to.value != 0U);
        result.after = graph_editor_command_availability(editor_state);
        return result;
    }

    inline GraphEditorStateCommandResult set_graph_editor_pending_link_target(
        GraphEditorState& editor_state,
        PortId candidate_to)
    {
        GraphEditorStateCommandResult result;
        result.before = graph_editor_command_availability(editor_state);
        const GraphEditorPendingLink previous = editor_state.pending_link();
        result.result = editor_state.set_pending_link_target(candidate_to);
        result.changed = result.success() && previous.candidate_to != candidate_to;
        result.after = graph_editor_command_availability(editor_state);
        return result;
    }

    inline GraphEditorStateCommandResult clear_graph_editor_selection(
        GraphEditorState& editor_state)
    {
        GraphEditorStateCommandResult result;
        result.before = graph_editor_command_availability(editor_state);
        if (!result.before.clear_selection) {
            result.result = Result::InvalidArgument;
            result.after = result.before;
            return result;
        }

        editor_state.clear_selection();
        result.changed = true;
        result.after = graph_editor_command_availability(editor_state);
        return result;
    }

    inline GraphEditorStateCommandResult clear_graph_editor_hover(
        GraphEditorState& editor_state)
    {
        GraphEditorStateCommandResult result;
        result.before = graph_editor_command_availability(editor_state);
        if (!result.before.clear_hover) {
            result.result = Result::InvalidArgument;
            result.after = result.before;
            return result;
        }

        editor_state.clear_hovered();
        result.changed = true;
        result.after = graph_editor_command_availability(editor_state);
        return result;
    }

    inline GraphEditorStateCommandResult cancel_graph_editor_pending_link(
        GraphEditorState& editor_state)
    {
        GraphEditorStateCommandResult result;
        result.before = graph_editor_command_availability(editor_state);
        if (!result.before.cancel_pending_link) {
            result.result = Result::InvalidArgument;
            result.after = result.before;
            return result;
        }

        editor_state.clear_pending_link();
        result.changed = true;
        result.after = graph_editor_command_availability(editor_state);
        return result;
    }

    inline GraphEditorStateCommandResult run_graph_editor_state_command(
        GraphEditorState& editor_state,
        GraphEditorCommandId command)
    {
        switch (command) {
        case GraphEditorCommandId::ClearSelection:
            return clear_graph_editor_selection(editor_state);
        case GraphEditorCommandId::ClearHover:
            return clear_graph_editor_hover(editor_state);
        case GraphEditorCommandId::CancelPendingLink:
            return cancel_graph_editor_pending_link(editor_state);
        }

        GraphEditorStateCommandResult result;
        result.before = graph_editor_command_availability(editor_state);
        result.result = Result::InvalidArgument;
        result.after = result.before;
        return result;
    }
}
