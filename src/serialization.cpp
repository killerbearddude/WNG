#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <wng/serialization.hpp>

namespace
{
    bool is_supported_version(const wng::GraphDtoVersion& version)
    {
        return version.major == 0U && version.minor == 2U && version.patch == 0U;
    }

    bool is_valid_port_kind(wng::PortKind kind)
    {
        return kind == wng::PortKind::Input || kind == wng::PortKind::Output;
    }

    bool is_finite(wng::Vec2 value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    bool types_are_compatible(const std::string& from_type, const std::string& to_type)
    {
        return from_type.empty() || to_type.empty() ||
               from_type == "any" || to_type == "any" ||
               from_type == to_type;
    }

    bool contains_node_id(const std::vector<wng::NodeId>& ids, wng::NodeId id)
    {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }

    bool contains_port_id(const std::vector<wng::PortId>& ids, wng::PortId id)
    {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }

    bool contains_link_id(const std::vector<wng::LinkId>& ids, wng::LinkId id)
    {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }

    const wng::NodeDto* find_node_dto(const wng::GraphDto& dto, wng::NodeId id)
    {
        for (const wng::NodeDto& node : dto.nodes) {
            if (node.id == id) {
                return &node;
            }
        }
        return nullptr;
    }

    const wng::PortDto* find_port_dto(const wng::GraphDto& dto, wng::PortId id)
    {
        for (const wng::PortDto& port : dto.ports) {
            if (port.id == id) {
                return &port;
            }
        }
        return nullptr;
    }

    bool node_declares_port(const wng::NodeDto& node, const wng::PortDto& port)
    {
        if (port.kind == wng::PortKind::Input) {
            return contains_port_id(node.inputs, port.id);
        }
        if (port.kind == wng::PortKind::Output) {
            return contains_port_id(node.outputs, port.id);
        }
        return false;
    }

    wng::Result validate_port_list(
        const wng::GraphDto& dto,
        const wng::NodeDto& node,
        const std::vector<wng::PortId>& ports,
        wng::PortKind expected_kind)
    {
        std::vector<wng::PortId> seen;
        seen.reserve(ports.size());

        for (wng::PortId id : ports) {
            if (contains_port_id(seen, id)) {
                return wng::Result::AlreadyExists;
            }
            seen.push_back(id);

            const wng::PortDto* port = find_port_dto(dto, id);
            if (port == nullptr) {
                return wng::Result::NotFound;
            }
            if (port->node != node.id) {
                return wng::Result::InvalidConnection;
            }
            if (port->kind != expected_kind) {
                return wng::Result::InvalidConnection;
            }
        }

        return wng::Result::Ok;
    }

    std::uint32_t restored_next_id(std::uint32_t max_id)
    {
        if (max_id == 0U) {
            return 1U;
        }
        return max_id + 1U;
    }
}

namespace wng
{
    Result export_graph(const Graph& graph, GraphDto* out_graph)
    {
        if (out_graph == nullptr) {
            return Result::InvalidArgument;
        }

        try {
            GraphDto exported;
            exported.version = GraphDtoVersion {};

            exported.nodes.reserve(graph.nodes().size());
            exported.ports.reserve(graph.ports().size());
            exported.links.reserve(graph.links().size());

            for (const Node& node : graph.nodes()) {
                NodeDto dto;
                dto.id = node.id;
                dto.type = node.type;
                dto.title = node.title;
                dto.position = node.position;
                dto.size = node.size;
                dto.inputs = node.inputs;
                dto.outputs = node.outputs;
                dto.visible = node.visible;
                dto.enabled = node.enabled;
                exported.nodes.push_back(std::move(dto));
            }

            for (const Port& port : graph.ports()) {
                PortDto dto;
                dto.id = port.id;
                dto.node = port.node;
                dto.kind = port.kind;
                dto.name = port.name;
                dto.type = port.type;
                dto.visible = port.visible;
                dto.enabled = port.enabled;
                exported.ports.push_back(std::move(dto));
            }

            for (const Link& link : graph.links()) {
                LinkDto dto;
                dto.id = link.id;
                dto.from = link.from;
                dto.to = link.to;
                dto.visible = link.visible;
                dto.enabled = link.enabled;
                exported.links.push_back(dto);
            }

            out_graph->version = exported.version;
            out_graph->nodes.swap(exported.nodes);
            out_graph->ports.swap(exported.ports);
            out_graph->links.swap(exported.links);

            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    Result import_graph(const GraphDto& dto, Graph* out_graph)
    {
        if (out_graph == nullptr) {
            return Result::InvalidArgument;
        }

        try {
            if (!is_supported_version(dto.version)) {
                return Result::InvalidArgument;
            }

            std::vector<NodeId> node_ids;
            std::vector<PortId> port_ids;
            std::vector<LinkId> link_ids;
            node_ids.reserve(dto.nodes.size());
            port_ids.reserve(dto.ports.size());
            link_ids.reserve(dto.links.size());

            std::uint32_t max_node_id = 0U;
            std::uint32_t max_port_id = 0U;
            std::uint32_t max_link_id = 0U;

            for (const NodeDto& node : dto.nodes) {
                if (node.id.value == 0U || node.id.value == std::numeric_limits<std::uint32_t>::max()) {
                    return Result::InvalidArgument;
                }
                if (contains_node_id(node_ids, node.id)) {
                    return Result::AlreadyExists;
                }
                if (!is_finite(node.position) || !is_finite(node.size)) {
                    return Result::InvalidArgument;
                }
                if (node.size.x < 0.0f || node.size.y < 0.0f) {
                    return Result::InvalidArgument;
                }

                node_ids.push_back(node.id);
                max_node_id = std::max(max_node_id, node.id.value);
            }

            for (const PortDto& port : dto.ports) {
                if (port.id.value == 0U || port.id.value == std::numeric_limits<std::uint32_t>::max()) {
                    return Result::InvalidArgument;
                }
                if (contains_port_id(port_ids, port.id)) {
                    return Result::AlreadyExists;
                }
                if (!is_valid_port_kind(port.kind)) {
                    return Result::InvalidArgument;
                }
                if (find_node_dto(dto, port.node) == nullptr) {
                    return Result::NotFound;
                }

                port_ids.push_back(port.id);
                max_port_id = std::max(max_port_id, port.id.value);
            }

            for (const LinkDto& link : dto.links) {
                if (link.id.value == 0U || link.id.value == std::numeric_limits<std::uint32_t>::max()) {
                    return Result::InvalidArgument;
                }
                if (contains_link_id(link_ids, link.id)) {
                    return Result::AlreadyExists;
                }

                link_ids.push_back(link.id);
                max_link_id = std::max(max_link_id, link.id.value);
            }

            for (const NodeDto& node : dto.nodes) {
                Result result = validate_port_list(dto, node, node.inputs, PortKind::Input);
                if (result != Result::Ok) {
                    return result;
                }

                result = validate_port_list(dto, node, node.outputs, PortKind::Output);
                if (result != Result::Ok) {
                    return result;
                }
            }

            for (const PortDto& port : dto.ports) {
                const NodeDto* node = find_node_dto(dto, port.node);
                if (node == nullptr) {
                    return Result::NotFound;
                }
                if (!node_declares_port(*node, port)) {
                    return Result::InvalidConnection;
                }
            }

            for (std::vector<LinkDto>::size_type i = 0; i < dto.links.size(); ++i) {
                const LinkDto& link = dto.links[i];
                const PortDto* from_port = find_port_dto(dto, link.from);
                const PortDto* to_port = find_port_dto(dto, link.to);
                if (from_port == nullptr || to_port == nullptr) {
                    return Result::NotFound;
                }
                if (link.from == link.to) {
                    return Result::InvalidConnection;
                }
                if (from_port->kind != PortKind::Output || to_port->kind != PortKind::Input) {
                    return Result::InvalidConnection;
                }
                if (from_port->node == to_port->node) {
                    return Result::InvalidConnection;
                }
                if (!types_are_compatible(from_port->type, to_port->type)) {
                    return Result::InvalidConnection;
                }

                for (std::vector<LinkDto>::size_type previous = 0; previous < i; ++previous) {
                    const LinkDto& previous_link = dto.links[previous];
                    if (previous_link.from == link.from && previous_link.to == link.to) {
                        return Result::AlreadyExists;
                    }
                    if (previous_link.to == link.to) {
                        return Result::InvalidConnection;
                    }
                }
            }

            std::vector<Node> imported_nodes;
            std::vector<Port> imported_ports;
            std::vector<Link> imported_links;
            imported_nodes.reserve(dto.nodes.size());
            imported_ports.reserve(dto.ports.size());
            imported_links.reserve(dto.links.size());

            for (const NodeDto& node_dto : dto.nodes) {
                Node node;
                node.id = node_dto.id;
                node.type = node_dto.type;
                node.title = node_dto.title;
                node.position = node_dto.position;
                node.size = node_dto.size;
                node.inputs = node_dto.inputs;
                node.outputs = node_dto.outputs;
                node.visible = node_dto.visible;
                node.enabled = node_dto.enabled;
                imported_nodes.push_back(std::move(node));
            }

            for (const PortDto& port_dto : dto.ports) {
                Port port;
                port.id = port_dto.id;
                port.node = port_dto.node;
                port.kind = port_dto.kind;
                port.name = port_dto.name;
                port.type = port_dto.type;
                port.visible = port_dto.visible;
                port.enabled = port_dto.enabled;
                imported_ports.push_back(std::move(port));
            }

            for (const LinkDto& link_dto : dto.links) {
                Link link;
                link.id = link_dto.id;
                link.from = link_dto.from;
                link.to = link_dto.to;
                link.visible = link_dto.visible;
                link.enabled = link_dto.enabled;
                imported_links.push_back(link);
            }

            out_graph->nodes_.swap(imported_nodes);
            out_graph->ports_.swap(imported_ports);
            out_graph->links_.swap(imported_links);
            out_graph->next_node_id_ = restored_next_id(max_node_id);
            out_graph->next_port_id_ = restored_next_id(max_port_id);
            out_graph->next_link_id_ = restored_next_id(max_link_id);

            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }
}
