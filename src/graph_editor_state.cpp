// Implements WNG editor-facing graph state with stable IDs only.
// This module deliberately avoids UI, WPL, rendering, hit testing, and platform
// input ownership so higher editor layers can build on it without coupling the
// graph core to presentation concerns.

#include <wng/graph_editor_state.hpp>

#include <algorithm>
#include <new>

namespace
{
    bool valid(wng::NodeId id)
    {
        return id.value != 0;
    }

    bool valid(wng::PortId id)
    {
        return id.value != 0;
    }

    bool valid(wng::LinkId id)
    {
        return id.value != 0;
    }

    template <typename Id>
    bool contains_id(const std::vector<Id>& ids, Id id)
    {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }

    template <typename Id>
    void erase_id(std::vector<Id>& ids, Id id)
    {
        ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
    }

    template <typename Id>
    void erase_any(std::vector<Id>& ids, const std::vector<Id>& removed)
    {
        for (Id id : removed) {
            erase_id(ids, id);
        }
    }
}

namespace wng
{
    bool GraphEditorElement::valid() const
    {
        switch (kind) {
        case GraphEditorElementKind::None:
            return false;
        case GraphEditorElementKind::Node:
            return node.value != 0;
        case GraphEditorElementKind::Port:
            return port.value != 0;
        case GraphEditorElementKind::Link:
            return link.value != 0;
        }

        return false;
    }

    const std::vector<NodeId>& GraphEditorState::selected_nodes() const
    {
        return selected_nodes_;
    }

    const std::vector<PortId>& GraphEditorState::selected_ports() const
    {
        return selected_ports_;
    }

    const std::vector<LinkId>& GraphEditorState::selected_links() const
    {
        return selected_links_;
    }

    GraphEditorElement GraphEditorState::hovered() const
    {
        return hovered_;
    }

    GraphEditorPendingLink GraphEditorState::pending_link() const
    {
        return pending_link_;
    }

    Result GraphEditorState::select_node(NodeId node)
    {
        if (!valid(node)) {
            return Result::InvalidArgument;
        }
        if (node_selected(node)) {
            return Result::AlreadyExists;
        }

        try {
            selected_nodes_.push_back(node);
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    Result GraphEditorState::select_port(PortId port)
    {
        if (!valid(port)) {
            return Result::InvalidArgument;
        }
        if (port_selected(port)) {
            return Result::AlreadyExists;
        }

        try {
            selected_ports_.push_back(port);
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    Result GraphEditorState::select_link(LinkId link)
    {
        if (!valid(link)) {
            return Result::InvalidArgument;
        }
        if (link_selected(link)) {
            return Result::AlreadyExists;
        }

        try {
            selected_links_.push_back(link);
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    void GraphEditorState::deselect_node(NodeId node)
    {
        erase_id(selected_nodes_, node);
    }

    void GraphEditorState::deselect_port(PortId port)
    {
        erase_id(selected_ports_, port);
    }

    void GraphEditorState::deselect_link(LinkId link)
    {
        erase_id(selected_links_, link);
    }

    void GraphEditorState::clear_selection()
    {
        selected_nodes_.clear();
        selected_ports_.clear();
        selected_links_.clear();
    }

    bool GraphEditorState::node_selected(NodeId node) const
    {
        return contains_id(selected_nodes_, node);
    }

    bool GraphEditorState::port_selected(PortId port) const
    {
        return contains_id(selected_ports_, port);
    }

    bool GraphEditorState::link_selected(LinkId link) const
    {
        return contains_id(selected_links_, link);
    }

    Result GraphEditorState::set_hovered_node(NodeId node)
    {
        if (!valid(node)) {
            return Result::InvalidArgument;
        }

        hovered_ = GraphEditorElement {};
        hovered_.kind = GraphEditorElementKind::Node;
        hovered_.node = node;
        return Result::Ok;
    }

    Result GraphEditorState::set_hovered_port(PortId port)
    {
        if (!valid(port)) {
            return Result::InvalidArgument;
        }

        hovered_ = GraphEditorElement {};
        hovered_.kind = GraphEditorElementKind::Port;
        hovered_.port = port;
        return Result::Ok;
    }

    Result GraphEditorState::set_hovered_link(LinkId link)
    {
        if (!valid(link)) {
            return Result::InvalidArgument;
        }

        hovered_ = GraphEditorElement {};
        hovered_.kind = GraphEditorElementKind::Link;
        hovered_.link = link;
        return Result::Ok;
    }

    void GraphEditorState::clear_hovered()
    {
        hovered_ = GraphEditorElement {};
    }

    Result GraphEditorState::begin_pending_link(PortId from)
    {
        if (!valid(from)) {
            return Result::InvalidArgument;
        }

        pending_link_ = GraphEditorPendingLink {};
        pending_link_.active = true;
        pending_link_.from = from;
        return Result::Ok;
    }

    Result GraphEditorState::set_pending_link_target(PortId candidate_to)
    {
        if (!pending_link_.active || !valid(candidate_to)) {
            return Result::InvalidArgument;
        }

        pending_link_.candidate_to = candidate_to;
        return Result::Ok;
    }

    void GraphEditorState::clear_pending_link_target()
    {
        pending_link_.candidate_to = PortId {};
    }

    void GraphEditorState::clear_pending_link()
    {
        pending_link_ = GraphEditorPendingLink {};
    }

    void GraphEditorState::apply_mutation_summary(const GraphMutationSummary& summary)
    {
        erase_any(selected_nodes_, summary.removed_nodes);
        erase_any(selected_ports_, summary.removed_ports);
        erase_any(selected_links_, summary.removed_links);

        if (hovered_.kind == GraphEditorElementKind::Node &&
            contains_id(summary.removed_nodes, hovered_.node)) {
            clear_hovered();
        } else if (hovered_.kind == GraphEditorElementKind::Port &&
            contains_id(summary.removed_ports, hovered_.port)) {
            clear_hovered();
        } else if (hovered_.kind == GraphEditorElementKind::Link &&
            contains_id(summary.removed_links, hovered_.link)) {
            clear_hovered();
        }

        // Pending link state references ports only. Any removed source or target
        // port invalidates that interaction because higher layers must re-hit-test
        // against the current graph before completing a link.
        if (pending_link_.active &&
            (contains_id(summary.removed_ports, pending_link_.from) ||
             contains_id(summary.removed_ports, pending_link_.candidate_to))) {
            clear_pending_link();
        }
    }
}
