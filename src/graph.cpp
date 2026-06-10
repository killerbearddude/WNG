#include <wng/graph.hpp>

#include <algorithm>
#include <cmath>
#include <new>
#include <utility>

namespace wng
{
    namespace
    {
        bool is_finite(Vec2 value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        bool is_valid_port_kind(PortKind kind)
        {
            return kind == PortKind::Input || kind == PortKind::Output;
        }

        bool types_are_compatible(const std::string& from_type, const std::string& to_type)
        {
            return from_type.empty() || to_type.empty() ||
                   from_type == "any" || to_type == "any" ||
                   from_type == to_type;
        }

        ConnectionValidation reject(Result result)
        {
            return ConnectionValidation { ConnectionStatus::Rejected, result };
        }

        ConnectionValidation allow()
        {
            return ConnectionValidation { ConnectionStatus::Allowed, Result::Ok };
        }

        bool contains_port(const std::vector<PortId>& ports, PortId id)
        {
            return std::find(ports.begin(), ports.end(), id) != ports.end();
        }

        void erase_port_id(std::vector<PortId>& ports, PortId id)
        {
            ports.erase(std::remove(ports.begin(), ports.end(), id), ports.end());
        }

        bool link_touches_any_port(const Link& link, const std::vector<PortId>& ports)
        {
            return contains_port(ports, link.from) || contains_port(ports, link.to);
        }
    }

    Result Graph::create_node(const NodeDesc& desc, NodeId* out_id)
    {
        if (out_id == nullptr) {
            return Result::InvalidArgument;
        }
        if (!is_finite(desc.position) || !is_finite(desc.size)) {
            return Result::InvalidArgument;
        }
        if (desc.size.x < 0.0f || desc.size.y < 0.0f) {
            return Result::InvalidArgument;
        }

        try {
            Node node;
            node.id = NodeId { next_node_id_ };
            node.type = desc.type;
            node.title = desc.title;
            node.position = desc.position;
            node.size = desc.size;
            node.visible = desc.visible;
            node.enabled = desc.enabled;

            nodes_.reserve(nodes_.size() + 1U);
            nodes_.push_back(std::move(node));
            *out_id = NodeId { next_node_id_ };
            ++next_node_id_;
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    Result Graph::destroy_node(NodeId id, GraphMutationSummary* out_summary)
    {
        if (id.value == 0U) {
            return Result::InvalidArgument;
        }

        Node* node = find_node(id);
        if (node == nullptr) {
            return Result::NotFound;
        }

        try {
            GraphMutationSummary prepared;
            prepared.removed_nodes.reserve(1U);
            prepared.removed_ports.reserve(node->inputs.size() + node->outputs.size());
            prepared.removed_links.reserve(links_.size());

            prepared.removed_nodes.push_back(id);
            for (PortId port_id : node->inputs) {
                prepared.removed_ports.push_back(port_id);
            }
            for (PortId port_id : node->outputs) {
                prepared.removed_ports.push_back(port_id);
            }
            for (const Link& link : links_) {
                if (link_touches_any_port(link, prepared.removed_ports)) {
                    prepared.removed_links.push_back(link.id);
                }
            }

            links_.erase(
                std::remove_if(
                    links_.begin(),
                    links_.end(),
                    [&prepared](const Link& link) {
                        return std::find(prepared.removed_links.begin(), prepared.removed_links.end(), link.id) != prepared.removed_links.end();
                    }),
                links_.end());

            ports_.erase(
                std::remove_if(
                    ports_.begin(),
                    ports_.end(),
                    [&prepared](const Port& port) {
                        return std::find(prepared.removed_ports.begin(), prepared.removed_ports.end(), port.id) != prepared.removed_ports.end();
                    }),
                ports_.end());

            nodes_.erase(
                std::remove_if(
                    nodes_.begin(),
                    nodes_.end(),
                    [id](const Node& candidate) { return candidate.id == id; }),
                nodes_.end());

            if (out_summary != nullptr) {
                out_summary->removed_nodes.swap(prepared.removed_nodes);
                out_summary->removed_ports.swap(prepared.removed_ports);
                out_summary->removed_links.swap(prepared.removed_links);
            }
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    Result Graph::add_port(NodeId node_id, const PortDesc& desc, PortId* out_id)
    {
        if (out_id == nullptr) {
            return Result::InvalidArgument;
        }
        if (node_id.value == 0U) {
            return Result::InvalidArgument;
        }
        if (!is_valid_port_kind(desc.kind)) {
            return Result::InvalidArgument;
        }

        Node* node = find_node(node_id);
        if (node == nullptr) {
            return Result::NotFound;
        }

        try {
            Port port;
            port.id = PortId { next_port_id_ };
            port.node = node_id;
            port.kind = desc.kind;
            port.name = desc.name;
            port.type = desc.type;
            port.visible = desc.visible;
            port.enabled = desc.enabled;

            ports_.reserve(ports_.size() + 1U);
            if (desc.kind == PortKind::Input) {
                node->inputs.reserve(node->inputs.size() + 1U);
            } else {
                node->outputs.reserve(node->outputs.size() + 1U);
            }

            ports_.push_back(std::move(port));
            if (desc.kind == PortKind::Input) {
                node->inputs.push_back(PortId { next_port_id_ });
            } else {
                node->outputs.push_back(PortId { next_port_id_ });
            }

            *out_id = PortId { next_port_id_ };
            ++next_port_id_;
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    Result Graph::remove_port(PortId id, GraphMutationSummary* out_summary)
    {
        if (id.value == 0U) {
            return Result::InvalidArgument;
        }

        Port* port = find_port(id);
        if (port == nullptr) {
            return Result::NotFound;
        }

        const NodeId parent_node_id = port->node;
        const PortKind kind = port->kind;

        try {
            GraphMutationSummary prepared;
            prepared.removed_ports.reserve(1U);
            prepared.removed_links.reserve(links_.size());

            prepared.removed_ports.push_back(id);
            for (const Link& link : links_) {
                if (link.from == id || link.to == id) {
                    prepared.removed_links.push_back(link.id);
                }
            }

            links_.erase(
                std::remove_if(
                    links_.begin(),
                    links_.end(),
                    [&prepared](const Link& link) {
                        return std::find(prepared.removed_links.begin(), prepared.removed_links.end(), link.id) != prepared.removed_links.end();
                    }),
                links_.end());

            ports_.erase(
                std::remove_if(
                    ports_.begin(),
                    ports_.end(),
                    [id](const Port& candidate) { return candidate.id == id; }),
                ports_.end());

            Node* parent = find_node(parent_node_id);
            if (parent != nullptr) {
                if (kind == PortKind::Input) {
                    erase_port_id(parent->inputs, id);
                } else {
                    erase_port_id(parent->outputs, id);
                }
            }

            if (out_summary != nullptr) {
                out_summary->removed_nodes.swap(prepared.removed_nodes);
                out_summary->removed_ports.swap(prepared.removed_ports);
                out_summary->removed_links.swap(prepared.removed_links);
            }
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    Result Graph::create_link(PortId from, PortId to, LinkId* out_id)
    {
        if (out_id == nullptr) {
            return Result::InvalidArgument;
        }

        const ConnectionValidation validation = validate_connection(from, to);
        if (validation.status != ConnectionStatus::Allowed) {
            return validation.result;
        }

        try {
            Link link;
            link.id = LinkId { next_link_id_ };
            link.from = from;
            link.to = to;

            links_.reserve(links_.size() + 1U);
            links_.push_back(link);
            *out_id = LinkId { next_link_id_ };
            ++next_link_id_;
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    Result Graph::destroy_link(LinkId id, GraphMutationSummary* out_summary)
    {
        if (id.value == 0U) {
            return Result::InvalidArgument;
        }
        if (find_link(id) == nullptr) {
            return Result::NotFound;
        }

        try {
            GraphMutationSummary prepared;
            prepared.removed_links.reserve(1U);
            prepared.removed_links.push_back(id);

            links_.erase(
                std::remove_if(
                    links_.begin(),
                    links_.end(),
                    [id](const Link& candidate) { return candidate.id == id; }),
                links_.end());

            if (out_summary != nullptr) {
                out_summary->removed_nodes.swap(prepared.removed_nodes);
                out_summary->removed_ports.swap(prepared.removed_ports);
                out_summary->removed_links.swap(prepared.removed_links);
            }
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    ConnectionValidation Graph::validate_connection(PortId from, PortId to) const
    {
        if (from.value == 0U || to.value == 0U) {
            return reject(Result::InvalidArgument);
        }

        const Port* from_port = find_port(from);
        const Port* to_port = find_port(to);
        if (from_port == nullptr || to_port == nullptr) {
            return reject(Result::NotFound);
        }

        const Node* from_node = find_node(from_port->node);
        const Node* to_node = find_node(to_port->node);
        if (from_node == nullptr || to_node == nullptr) {
            return reject(Result::NotFound);
        }

        if (from == to) {
            return reject(Result::InvalidConnection);
        }
        if (from_port->node == to_port->node) {
            return reject(Result::InvalidConnection);
        }
        if (from_port->kind != PortKind::Output || to_port->kind != PortKind::Input) {
            return reject(Result::InvalidConnection);
        }

        for (const Link& link : links_) {
            if (link.from == from && link.to == to) {
                return reject(Result::AlreadyExists);
            }
        }
        for (const Link& link : links_) {
            if (link.to == to) {
                return reject(Result::InvalidConnection);
            }
        }

        if (!types_are_compatible(from_port->type, to_port->type)) {
            return reject(Result::InvalidConnection);
        }
        if (!from_node->enabled || !to_node->enabled) {
            return reject(Result::InvalidConnection);
        }
        if (!from_port->enabled || !to_port->enabled) {
            return reject(Result::InvalidConnection);
        }

        return allow();
    }

    Node* Graph::find_node(NodeId id)
    {
        if (id.value == 0U) {
            return nullptr;
        }
        for (Node& node : nodes_) {
            if (node.id == id) {
                return &node;
            }
        }
        return nullptr;
    }

    Port* Graph::find_port(PortId id)
    {
        if (id.value == 0U) {
            return nullptr;
        }
        for (Port& port : ports_) {
            if (port.id == id) {
                return &port;
            }
        }
        return nullptr;
    }

    Link* Graph::find_link(LinkId id)
    {
        if (id.value == 0U) {
            return nullptr;
        }
        for (Link& link : links_) {
            if (link.id == id) {
                return &link;
            }
        }
        return nullptr;
    }

    const Node* Graph::find_node(NodeId id) const
    {
        if (id.value == 0U) {
            return nullptr;
        }
        for (const Node& node : nodes_) {
            if (node.id == id) {
                return &node;
            }
        }
        return nullptr;
    }

    const Port* Graph::find_port(PortId id) const
    {
        if (id.value == 0U) {
            return nullptr;
        }
        for (const Port& port : ports_) {
            if (port.id == id) {
                return &port;
            }
        }
        return nullptr;
    }

    const Link* Graph::find_link(LinkId id) const
    {
        if (id.value == 0U) {
            return nullptr;
        }
        for (const Link& link : links_) {
            if (link.id == id) {
                return &link;
            }
        }
        return nullptr;
    }

    const std::vector<Node>& Graph::nodes() const
    {
        return nodes_;
    }

    const std::vector<Port>& Graph::ports() const
    {
        return ports_;
    }

    const std::vector<Link>& Graph::links() const
    {
        return links_;
    }
}
