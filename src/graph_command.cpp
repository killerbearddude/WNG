// Implements command-style graph mutation helpers and batch metadata for WNG.
// These helpers execute individual public Graph mutations or group their records;
// they do not own command history, rollback, replay, or undo/redo stacks.

#include <vector>

#include <wng/graph_command.hpp>
#include <wng/schema_mutation.hpp>

namespace
{
    void set_result(wng::GraphCommandResult& command, wng::Result result)
    {
        command.result = result;
        command.record.result = result;
    }

    bool contains_node_id(const std::vector<wng::NodeId>& ids, wng::NodeId id)
    {
        for (wng::NodeId existing : ids) {
            if (existing == id) {
                return true;
            }
        }

        return false;
    }

    bool contains_port_id(const std::vector<wng::PortId>& ids, wng::PortId id)
    {
        for (wng::PortId existing : ids) {
            if (existing == id) {
                return true;
            }
        }

        return false;
    }

    bool contains_link_id(const std::vector<wng::LinkId>& ids, wng::LinkId id)
    {
        for (wng::LinkId existing : ids) {
            if (existing == id) {
                return true;
            }
        }

        return false;
    }

    void append_node_id(std::vector<wng::NodeId>& ids, wng::NodeId id)
    {
        if (id != wng::NodeId {} && !contains_node_id(ids, id)) {
            ids.push_back(id);
        }
    }

    void append_port_id(std::vector<wng::PortId>& ids, wng::PortId id)
    {
        if (id != wng::PortId {} && !contains_port_id(ids, id)) {
            ids.push_back(id);
        }
    }

    void append_link_id(std::vector<wng::LinkId>& ids, wng::LinkId id)
    {
        if (id != wng::LinkId {} && !contains_link_id(ids, id)) {
            ids.push_back(id);
        }
    }

    void snapshot_created_node(
        const wng::Graph& graph,
        wng::NodeId node,
        wng::GraphCommandRecord& record)
    {
        const wng::Node* created_node = graph.find_node(node);
        if (created_node != nullptr) {
            record.created_nodes.push_back(*created_node);
        }
    }

    void snapshot_created_port(
        const wng::Graph& graph,
        wng::PortId port,
        wng::GraphCommandRecord& record)
    {
        const wng::Port* created_port = graph.find_port(port);
        if (created_port != nullptr) {
            record.created_ports.push_back(*created_port);
        }
    }

    void snapshot_created_link(
        const wng::Graph& graph,
        wng::LinkId link,
        wng::GraphCommandRecord& record)
    {
        const wng::Link* created_link = graph.find_link(link);
        if (created_link != nullptr) {
            record.created_links.push_back(*created_link);
        }
    }

    void snapshot_created_node_with_ports(
        const wng::Graph& graph,
        wng::NodeId node,
        wng::GraphCommandRecord& record)
    {
        const wng::Node* created_node = graph.find_node(node);
        if (created_node == nullptr) {
            return;
        }

        record.created_nodes.push_back(*created_node);

        for (const wng::Port& port : graph.ports()) {
            if (port.node == node) {
                record.created_ports.push_back(port);
            }
        }
    }

    bool port_owned_by_node(const wng::Port& port, wng::NodeId node)
    {
        return port.node == node;
    }

    bool link_uses_port(const wng::Link& link, wng::PortId port)
    {
        return link.from == port || link.to == port;
    }

    bool link_uses_any_port(const wng::Link& link, const std::vector<wng::Port>& ports)
    {
        for (const wng::Port& port : ports) {
            if (link_uses_port(link, port.id)) {
                return true;
            }
        }

        return false;
    }

    void snapshot_node_removal(
        const wng::Graph& graph,
        wng::NodeId node,
        wng::GraphCommandRecord& record)
    {
        const wng::Node* existing_node = graph.find_node(node);
        if (existing_node == nullptr) {
            return;
        }

        record.removed_nodes.push_back(*existing_node);

        // Destructive commands snapshot observable state before mutating Graph.
        // That keeps the future undo layer independent from private Graph storage.
        for (const wng::Port& port : graph.ports()) {
            if (port_owned_by_node(port, node)) {
                record.removed_ports.push_back(port);
            }
        }

        for (const wng::Link& link : graph.links()) {
            if (link_uses_any_port(link, record.removed_ports)) {
                record.removed_links.push_back(link);
            }
        }
    }

    void snapshot_port_removal(
        const wng::Graph& graph,
        wng::PortId port,
        wng::GraphCommandRecord& record)
    {
        const wng::Port* existing_port = graph.find_port(port);
        if (existing_port == nullptr) {
            return;
        }

        record.removed_ports.push_back(*existing_port);

        // Removing a port may remove dependent links. Snapshot those links before
        // delegating to Graph::remove_port so undo data remains public and stable.
        for (const wng::Link& link : graph.links()) {
            if (link_uses_port(link, port)) {
                record.removed_links.push_back(link);
            }
        }
    }

    void snapshot_link_removal(
        const wng::Graph& graph,
        wng::LinkId link,
        wng::GraphCommandRecord& record)
    {
        const wng::Link* existing_link = graph.find_link(link);
        if (existing_link != nullptr) {
            record.removed_links.push_back(*existing_link);
        }
    }
}

namespace wng
{
    bool GraphCommandResult::success() const
    {
        return result == Result::Ok;
    }

    bool GraphCommandBatch::success() const
    {
        return result == Result::Ok;
    }

    bool GraphCommandBatch::empty() const
    {
        return records.empty();
    }

    void append_command_result(
        GraphCommandBatch& batch,
        const GraphCommandResult& result)
    {
        batch.records.push_back(result.record);

        // Preserve the first failure as the aggregate batch result. Later records
        // remain useful metadata, but they should not hide the original failure.
        if (batch.result == Result::Ok) {
            batch.result = result.result;
        }
    }

    const GraphCommandRecord* first_failed_command(
        const GraphCommandBatch& batch)
    {
        for (const GraphCommandRecord& record : batch.records) {
            if (record.result != Result::Ok) {
                return &record;
            }
        }

        return nullptr;
    }

    std::vector<NodeId> created_nodes(const GraphCommandBatch& batch)
    {
        std::vector<NodeId> ids;

        // Batch collectors intentionally read only command-record snapshots. They
        // are safe to call after Graph has changed and they do not mutate Graph.
        for (const GraphCommandRecord& record : batch.records) {
            for (const Node& node : record.created_nodes) {
                append_node_id(ids, node.id);
            }
        }

        return ids;
    }

    std::vector<PortId> created_ports(const GraphCommandBatch& batch)
    {
        std::vector<PortId> ids;

        // Snapshot order is the deterministic source of truth for batch metadata;
        // no live Graph lookup is needed or permitted here.
        for (const GraphCommandRecord& record : batch.records) {
            for (const Port& port : record.created_ports) {
                append_port_id(ids, port.id);
            }
        }

        return ids;
    }

    std::vector<LinkId> created_links(const GraphCommandBatch& batch)
    {
        std::vector<LinkId> ids;

        for (const GraphCommandRecord& record : batch.records) {
            for (const Link& link : record.created_links) {
                append_link_id(ids, link.id);
            }
        }

        return ids;
    }

    std::vector<NodeId> removed_nodes(const GraphCommandBatch& batch)
    {
        std::vector<NodeId> ids;

        for (const GraphCommandRecord& record : batch.records) {
            for (const Node& node : record.removed_nodes) {
                append_node_id(ids, node.id);
            }
        }

        return ids;
    }

    std::vector<PortId> removed_ports(const GraphCommandBatch& batch)
    {
        std::vector<PortId> ids;

        for (const GraphCommandRecord& record : batch.records) {
            for (const Port& port : record.removed_ports) {
                append_port_id(ids, port.id);
            }
        }

        return ids;
    }

    std::vector<LinkId> removed_links(const GraphCommandBatch& batch)
    {
        std::vector<LinkId> ids;

        for (const GraphCommandRecord& record : batch.records) {
            for (const Link& link : record.removed_links) {
                append_link_id(ids, link.id);
            }
        }

        return ids;
    }

    GraphCommandResult command_create_node(
        Graph& graph,
        const NodeDesc& desc)
    {
        GraphCommandResult command;
        command.record.kind = GraphCommandKind::CreateNode;
        command.record.node_desc = desc;

        NodeId node;
        const Result create_result = graph.create_node(desc, &node);
        set_result(command, create_result);

        if (create_result == Result::Ok) {
            command.record.node = node;
            snapshot_created_node(graph, node, command.record);
        }

        return command;
    }

    GraphCommandResult command_destroy_node(
        Graph& graph,
        NodeId node)
    {
        GraphCommandResult command;
        command.record.kind = GraphCommandKind::DestroyNode;
        command.record.node = node;

        snapshot_node_removal(graph, node, command.record);

        GraphMutationSummary summary;
        const Result destroy_result = graph.destroy_node(node, &summary);
        set_result(command, destroy_result);

        if (destroy_result == Result::Ok) {
            command.record.summary = summary;
        } else {
            command.record.removed_nodes.clear();
            command.record.removed_ports.clear();
            command.record.removed_links.clear();
        }

        return command;
    }

    GraphCommandResult command_add_port(
        Graph& graph,
        NodeId node,
        const PortDesc& desc)
    {
        GraphCommandResult command;
        command.record.kind = GraphCommandKind::AddPort;
        command.record.node = node;
        command.record.port_desc = desc;

        PortId port;
        const Result add_result = graph.add_port(node, desc, &port);
        set_result(command, add_result);

        if (add_result == Result::Ok) {
            command.record.port = port;
            snapshot_created_port(graph, port, command.record);
        }

        return command;
    }

    GraphCommandResult command_remove_port(
        Graph& graph,
        PortId port)
    {
        GraphCommandResult command;
        command.record.kind = GraphCommandKind::RemovePort;
        command.record.port = port;

        snapshot_port_removal(graph, port, command.record);

        GraphMutationSummary summary;
        const Result remove_result = graph.remove_port(port, &summary);
        set_result(command, remove_result);

        if (remove_result == Result::Ok) {
            command.record.summary = summary;
        } else {
            command.record.removed_ports.clear();
            command.record.removed_links.clear();
        }

        return command;
    }

    GraphCommandResult command_create_link(
        Graph& graph,
        PortId from,
        PortId to)
    {
        GraphCommandResult command;
        command.record.kind = GraphCommandKind::CreateLink;

        LinkId link;
        const Result create_result = graph.create_link(from, to, &link);
        set_result(command, create_result);

        if (create_result == Result::Ok) {
            command.record.link = link;
            snapshot_created_link(graph, link, command.record);
        }

        return command;
    }

    GraphCommandResult command_destroy_link(
        Graph& graph,
        LinkId link)
    {
        GraphCommandResult command;
        command.record.kind = GraphCommandKind::DestroyLink;
        command.record.link = link;

        snapshot_link_removal(graph, link, command.record);

        GraphMutationSummary summary;
        const Result destroy_result = graph.destroy_link(link, &summary);
        set_result(command, destroy_result);

        if (destroy_result == Result::Ok) {
            command.record.summary = summary;
        } else {
            command.record.removed_links.clear();
        }

        return command;
    }

    GraphCommandResult command_create_node(
        Graph& graph,
        const GraphSchema& schema,
        const NodeDesc& desc)
    {
        GraphCommandResult command;
        command.record.kind = GraphCommandKind::SchemaCreateNode;
        command.record.node_desc = desc;

        NodeId node;
        const Result create_result = wng::create_node(graph, schema, desc, &node);
        set_result(command, create_result);

        if (create_result == Result::Ok) {
            command.record.node = node;
            snapshot_created_node(graph, node, command.record);
        }

        return command;
    }

    GraphCommandResult command_instantiate_node(
        Graph& graph,
        const GraphSchema& schema,
        const NodeDesc& desc)
    {
        GraphCommandResult command;
        command.record.kind = GraphCommandKind::SchemaInstantiateNode;
        command.record.node_desc = desc;

        NodeId node;
        GraphMutationSummary rollback_summary;

        // Schema-aware command helpers delegate all schema policy to
        // schema_mutation.hpp. Commands record results, but they do not validate
        // schema rules themselves and they do not own command history.
        const Result instantiate_result =
            wng::instantiate_node(graph, schema, desc, &node, &rollback_summary);
        set_result(command, instantiate_result);

        if (instantiate_result == Result::Ok) {
            command.record.node = node;
            snapshot_created_node_with_ports(graph, node, command.record);
        } else {
            command.record.summary = rollback_summary;
        }

        return command;
    }

    GraphCommandResult command_add_port(
        Graph& graph,
        const GraphSchema& schema,
        NodeId node,
        const PortDesc& desc)
    {
        GraphCommandResult command;
        command.record.kind = GraphCommandKind::SchemaAddPort;
        command.record.node = node;
        command.record.port_desc = desc;

        PortId port;
        const Result add_result = wng::add_port(graph, schema, node, desc, &port);
        set_result(command, add_result);

        if (add_result == Result::Ok) {
            command.record.port = port;
            snapshot_created_port(graph, port, command.record);
        }

        return command;
    }

    GraphCommandResult command_create_link(
        Graph& graph,
        const GraphSchema& schema,
        PortId from,
        PortId to)
    {
        GraphCommandResult command;
        command.record.kind = GraphCommandKind::SchemaCreateLink;

        LinkId link;
        const Result create_result = wng::create_link(graph, schema, from, to, &link);
        set_result(command, create_result);

        if (create_result == Result::Ok) {
            command.record.link = link;
            snapshot_created_link(graph, link, command.record);
        }

        return command;
    }

}
