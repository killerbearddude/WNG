// Implements atomic restoration of captured graph object snapshots.
// Restoration is intentionally isolated from normal Graph creation APIs so
// regular mutations keep allocating fresh IDs while undo/redo can restore IDs.

#include <cmath>
#include <new>
#include <string>
#include <vector>

#include <wng/graph_restore.hpp>

#include <wng/serialization.hpp>
#include <wng/serialization_dto.hpp>

namespace
{
    bool is_valid_port_kind(wng::PortKind kind)
    {
        return kind == wng::PortKind::Input || kind == wng::PortKind::Output;
    }

    bool is_finite(wng::Vec2 value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    bool type_compatible(const std::string& a, const std::string& b)
    {
        return a == b || a.empty() || b.empty() || a == "any" || b == "any";
    }

    bool contains_node_id(const std::vector<wng::Node>& nodes, wng::NodeId id)
    {
        for (const wng::Node& node : nodes) {
            if (node.id == id) {
                return true;
            }
        }

        return false;
    }

    bool contains_port_id(const std::vector<wng::Port>& ports, wng::PortId id)
    {
        for (const wng::Port& port : ports) {
            if (port.id == id) {
                return true;
            }
        }

        return false;
    }

    bool contains_link_id(const std::vector<wng::Link>& links, wng::LinkId id)
    {
        for (const wng::Link& link : links) {
            if (link.id == id) {
                return true;
            }
        }

        return false;
    }

    bool dto_contains_node_id(const wng::GraphDto& dto, wng::NodeId id)
    {
        for (const wng::NodeDto& node : dto.nodes) {
            if (node.id == id) {
                return true;
            }
        }

        return false;
    }

    bool dto_contains_port_id(const wng::GraphDto& dto, wng::PortId id)
    {
        for (const wng::PortDto& port : dto.ports) {
            if (port.id == id) {
                return true;
            }
        }

        return false;
    }

    wng::NodeDto* find_node_in_dto(wng::GraphDto& dto, wng::NodeId id)
    {
        for (wng::NodeDto& node : dto.nodes) {
            if (node.id == id) {
                return &node;
            }
        }

        return nullptr;
    }

    const wng::PortDto* find_port_in_dto(const wng::GraphDto& dto, wng::PortId id)
    {
        for (const wng::PortDto& port : dto.ports) {
            if (port.id == id) {
                return &port;
            }
        }

        return nullptr;
    }

    bool node_size_valid(wng::Vec2 size)
    {
        return is_finite(size) && size.x >= 0.0f && size.y >= 0.0f;
    }

    wng::Result validate_snapshot_ids(
        const wng::Graph& graph,
        const wng::GraphObjectSnapshot& snapshot)
    {
        std::vector<wng::Node> seen_nodes;
        std::vector<wng::Port> seen_ports;
        std::vector<wng::Link> seen_links;

        for (const wng::Node& node : snapshot.nodes) {
            if (node.id == wng::NodeId {} || !is_finite(node.position) || !node_size_valid(node.size)) {
                return wng::Result::InvalidArgument;
            }
            if (contains_node_id(graph.nodes(), node.id) || contains_node_id(seen_nodes, node.id)) {
                return wng::Result::AlreadyExists;
            }
            seen_nodes.push_back(node);
        }

        for (const wng::Port& port : snapshot.ports) {
            if (port.id == wng::PortId {} || !is_valid_port_kind(port.kind)) {
                return wng::Result::InvalidArgument;
            }
            if (contains_port_id(graph.ports(), port.id) || contains_port_id(seen_ports, port.id)) {
                return wng::Result::AlreadyExists;
            }
            seen_ports.push_back(port);
        }

        for (const wng::Link& link : snapshot.links) {
            if (link.id == wng::LinkId {}) {
                return wng::Result::InvalidArgument;
            }
            if (contains_link_id(graph.links(), link.id) || contains_link_id(seen_links, link.id)) {
                return wng::Result::AlreadyExists;
            }
            seen_links.push_back(link);
        }

        return wng::Result::Ok;
    }

    wng::Result validate_restored_port_parents(
        const wng::GraphDto& dto,
        const wng::GraphObjectSnapshot& snapshot)
    {
        for (const wng::Port& port : snapshot.ports) {
            if (!dto_contains_node_id(dto, port.node)) {
                return wng::Result::NotFound;
            }
        }

        return wng::Result::Ok;
    }

    wng::Result validate_restored_link_endpoints(
        const wng::GraphDto& dto,
        const wng::GraphObjectSnapshot& snapshot)
    {
        for (const wng::Link& link : snapshot.links) {
            if (!dto_contains_port_id(dto, link.from) || !dto_contains_port_id(dto, link.to)) {
                return wng::Result::NotFound;
            }
        }

        return wng::Result::Ok;
    }

    wng::NodeDto make_node_dto(const wng::Node& node)
    {
        wng::NodeDto dto;
        dto.id = node.id;
        dto.type = node.type;
        dto.title = node.title;
        dto.position = node.position;
        dto.size = node.size;
        dto.inputs = node.inputs;
        dto.outputs = node.outputs;
        dto.visible = node.visible;
        dto.enabled = node.enabled;
        return dto;
    }

    wng::PortDto make_port_dto(const wng::Port& port)
    {
        wng::PortDto dto;
        dto.id = port.id;
        dto.node = port.node;
        dto.kind = port.kind;
        dto.name = port.name;
        dto.type = port.type;
        dto.visible = port.visible;
        dto.enabled = port.enabled;
        return dto;
    }

    wng::LinkDto make_link_dto(const wng::Link& link)
    {
        wng::LinkDto dto;
        dto.id = link.id;
        dto.from = link.from;
        dto.to = link.to;
        dto.visible = link.visible;
        dto.enabled = link.enabled;
        return dto;
    }

    void append_port_to_node(wng::NodeDto& node, const wng::PortDto& port)
    {
        std::vector<wng::PortId>& ports =
            port.kind == wng::PortKind::Input ? node.inputs : node.outputs;

        for (wng::PortId existing : ports) {
            if (existing == port.id) {
                return;
            }
        }

        ports.push_back(port.id);
    }

    void normalize_node_port_lists(wng::GraphDto& dto)
    {
        // Port ownership is authoritative after merging. Rebuilding node port
        // lists prevents stale snapshot-side vectors from breaking DTO import
        // while still restoring valid node/port identities and fields.
        for (wng::NodeDto& node : dto.nodes) {
            node.inputs.clear();
            node.outputs.clear();
        }

        for (const wng::PortDto& port : dto.ports) {
            wng::NodeDto* node = find_node_in_dto(dto, port.node);
            if (node != nullptr) {
                append_port_to_node(*node, port);
            }
        }
    }

    wng::Result validate_snapshot_links(const wng::GraphDto& dto)
    {
        for (std::vector<wng::LinkDto>::size_type i = 0; i < dto.links.size(); ++i) {
            const wng::LinkDto& link = dto.links[i];
            const wng::PortDto* from_port = find_port_in_dto(dto, link.from);
            const wng::PortDto* to_port = find_port_in_dto(dto, link.to);
            if (from_port == nullptr || to_port == nullptr) {
                return wng::Result::NotFound;
            }
            if (link.from == link.to ||
                from_port->kind != wng::PortKind::Output ||
                to_port->kind != wng::PortKind::Input ||
                from_port->node == to_port->node ||
                !type_compatible(from_port->type, to_port->type)) {
                return wng::Result::InvalidConnection;
            }

            for (std::vector<wng::LinkDto>::size_type previous = 0; previous < i; ++previous) {
                const wng::LinkDto& previous_link = dto.links[previous];
                if (previous_link.from == link.from && previous_link.to == link.to) {
                    return wng::Result::AlreadyExists;
                }
                if (previous_link.to == link.to) {
                    return wng::Result::InvalidConnection;
                }
            }
        }

        return wng::Result::Ok;
    }

    wng::Result merge_snapshot_into_dto(
        wng::GraphDto& dto,
        const wng::GraphObjectSnapshot& snapshot)
    {
        for (const wng::Node& node : snapshot.nodes) {
            dto.nodes.push_back(make_node_dto(node));
        }

        wng::Result reference_result = validate_restored_port_parents(dto, snapshot);
        if (reference_result != wng::Result::Ok) {
            return reference_result;
        }

        for (const wng::Port& port : snapshot.ports) {
            dto.ports.push_back(make_port_dto(port));
        }

        reference_result = validate_restored_link_endpoints(dto, snapshot);
        if (reference_result != wng::Result::Ok) {
            return reference_result;
        }

        for (const wng::Link& link : snapshot.links) {
            dto.links.push_back(make_link_dto(link));
        }

        normalize_node_port_lists(dto);
        return validate_snapshot_links(dto);
    }

    void publish_restored_ids(
        const wng::GraphObjectSnapshot& snapshot,
        wng::GraphRestoreResult& result)
    {
        for (const wng::Node& node : snapshot.nodes) {
            result.restored_nodes.push_back(node.id);
        }
        for (const wng::Port& port : snapshot.ports) {
            result.restored_ports.push_back(port.id);
        }
        for (const wng::Link& link : snapshot.links) {
            result.restored_links.push_back(link.id);
        }
    }

    wng::GraphRestoreResult failure(wng::Result result)
    {
        wng::GraphRestoreResult restore;
        restore.result = result;
        return restore;
    }
}

namespace wng
{
    bool GraphObjectSnapshot::empty() const
    {
        return nodes.empty() && ports.empty() && links.empty();
    }

    bool GraphRestoreResult::success() const
    {
        return result == Result::Ok;
    }

    GraphRestoreResult restore_graph_objects(
        Graph& graph,
        const GraphObjectSnapshot& snapshot)
    {
        try {
            GraphRestoreResult result;

            if (snapshot.empty()) {
                return result;
            }

            const Result id_result = validate_snapshot_ids(graph, snapshot);
            if (id_result != Result::Ok) {
                return failure(id_result);
            }

            GraphDto merged;
            Result result_code = export_graph(graph, &merged);
            if (result_code != Result::Ok) {
                return failure(result_code);
            }

            // DTO replacement gives the restore operation all-or-nothing behavior
            // without adding explicit-ID creation methods to Graph's public API.
            result_code = merge_snapshot_into_dto(merged, snapshot);
            if (result_code != Result::Ok) {
                return failure(result_code);
            }

            Graph replacement;
            result_code = import_graph(merged, &replacement);
            if (result_code != Result::Ok) {
                return failure(result_code);
            }

            graph = replacement;
            publish_restored_ids(snapshot, result);
            return result;
        } catch (const std::bad_alloc&) {
            return failure(Result::AllocationFailure);
        }
    }
}
