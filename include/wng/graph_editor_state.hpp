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
        bool delete_selection = false;
        bool clear_hover = false;
        bool cancel_pending_link = false;
        bool complete_pending_link = false;

        bool any_available() const;
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

    GraphEditorCommandAvailability graph_editor_command_availability(
        const GraphEditorState& editor_state);
}
