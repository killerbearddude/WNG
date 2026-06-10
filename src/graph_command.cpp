// Implements command-style graph mutation helpers for WNG.
// These helpers are not a command history manager: they execute one public Graph
// mutation and return a value record that a future undo/redo layer can store.

#include <vector>

#include <wng/graph_command.hpp>

namespace
{
    void set_result(wng::GraphCommandResult& command, wng::Result result)
    {
        command.result = result;
        command.record.result = result;
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
}
