// Exercises command-style mutation helpers and their snapshot records.
// These tests protect future undo/redo integration points without adding a
// history stack, command batching, or schema-aware command behavior.

#include <cassert>

#include <wng/graph_command.hpp>

namespace
{
    wng::NodeDesc make_node_desc(const char* title = "Node")
    {
        wng::NodeDesc desc;
        desc.type = "schema.unknown";
        desc.title = title;
        desc.position = wng::Vec2 { 1.0f, 2.0f };
        desc.size = wng::Vec2 { 100.0f, 50.0f };
        return desc;
    }

    wng::NodeId create_node(wng::Graph& graph, const char* title = "Node")
    {
        wng::NodeId node;
        assert(graph.create_node(make_node_desc(title), &node) == wng::Result::Ok);
        return node;
    }

    wng::PortDesc make_port_desc(
        wng::PortKind kind,
        const char* name,
        const char* type = "number")
    {
        wng::PortDesc desc;
        desc.kind = kind;
        desc.name = name;
        desc.type = type;
        return desc;
    }

    wng::PortId add_port(
        wng::Graph& graph,
        wng::NodeId node,
        wng::PortKind kind,
        const char* name)
    {
        wng::PortId port;
        assert(graph.add_port(node, make_port_desc(kind, name), &port) == wng::Result::Ok);
        return port;
    }

    wng::LinkId create_link(wng::Graph& graph, wng::PortId from, wng::PortId to)
    {
        wng::LinkId link;
        assert(graph.create_link(from, to, &link) == wng::Result::Ok);
        return link;
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

    bool contains_summary_port(const wng::GraphMutationSummary& summary, wng::PortId id)
    {
        for (wng::PortId port : summary.removed_ports) {
            if (port == id) {
                return true;
            }
        }

        return false;
    }

    bool contains_summary_link(const wng::GraphMutationSummary& summary, wng::LinkId id)
    {
        for (wng::LinkId link : summary.removed_links) {
            if (link == id) {
                return true;
            }
        }

        return false;
    }
}

int main()
{
    {
        // Create-node commands record the descriptor and created id so future
        // undo/redo code can reason from command output instead of probing Graph.
        wng::Graph graph;
        const wng::NodeDesc desc = make_node_desc("Created");

        const wng::GraphCommandResult result = wng::command_create_node(graph, desc);

        assert(result.result == wng::Result::Ok);
        assert(result.record.result == wng::Result::Ok);
        assert(result.record.kind == wng::GraphCommandKind::CreateNode);
        assert(result.record.node != wng::NodeId {});
        assert(result.record.node_desc.title == desc.title);
        assert(result.record.node_desc.type == desc.type);
        assert(result.success());
        assert(graph.find_node(result.record.node) != nullptr);
    }

    {
        // Create-node failures preserve the failure result and do not fabricate
        // inverse data for a node that Graph rejected.
        wng::Graph graph;
        wng::NodeDesc desc = make_node_desc("Invalid");
        desc.size = wng::Vec2 { -1.0f, 10.0f };

        const wng::GraphCommandResult result = wng::command_create_node(graph, desc);

        assert(result.result != wng::Result::Ok);
        assert(result.record.result == result.result);
        assert(result.record.kind == wng::GraphCommandKind::CreateNode);
        assert(result.record.node == wng::NodeId {});
        assert(!result.success());
        assert(graph.nodes().empty());
    }

    {
        // Add-port commands delegate to Graph and record the target node,
        // descriptor, and created port id.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph);
        const wng::PortDesc desc = make_port_desc(wng::PortKind::Output, "out");

        const wng::GraphCommandResult result = wng::command_add_port(graph, node, desc);

        assert(result.result == wng::Result::Ok);
        assert(result.record.kind == wng::GraphCommandKind::AddPort);
        assert(result.record.node == node);
        assert(result.record.port != wng::PortId {});
        assert(result.record.port_desc.kind == desc.kind);
        assert(result.record.port_desc.name == desc.name);
        assert(graph.find_port(result.record.port) != nullptr);
    }

    {
        // Missing-node failures are recorded without changing graph state.
        wng::Graph graph;
        const wng::PortDesc desc = make_port_desc(wng::PortKind::Input, "in");

        const wng::GraphCommandResult result =
            wng::command_add_port(graph, wng::NodeId { 999 }, desc);

        assert(result.result == wng::Result::NotFound);
        assert(result.record.result == result.result);
        assert(result.record.kind == wng::GraphCommandKind::AddPort);
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
    }

    {
        // Create-link commands record the created link id while relying on Graph
        // for all connection validation.
        wng::Graph graph;
        const wng::NodeId source = create_node(graph, "Source");
        const wng::NodeId target = create_node(graph, "Target");
        const wng::PortId output = add_port(graph, source, wng::PortKind::Output, "out");
        const wng::PortId input = add_port(graph, target, wng::PortKind::Input, "in");

        const wng::GraphCommandResult result = wng::command_create_link(graph, output, input);

        assert(result.result == wng::Result::Ok);
        assert(result.record.kind == wng::GraphCommandKind::CreateLink);
        assert(result.record.link != wng::LinkId {});
        assert(graph.find_link(result.record.link) != nullptr);
    }

    {
        // Invalid connections stay Graph policy. The command wrapper records the
        // failure but does not duplicate or bypass validation.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph);
        const wng::PortId input_a = add_port(graph, node, wng::PortKind::Input, "a");
        const wng::PortId input_b = add_port(graph, node, wng::PortKind::Input, "b");

        const wng::GraphCommandResult result = wng::command_create_link(graph, input_a, input_b);

        assert(result.result == wng::Result::InvalidConnection);
        assert(result.record.result == result.result);
        assert(result.record.kind == wng::GraphCommandKind::CreateLink);
        assert(graph.links().empty());
    }

    {
        // Destroy-link commands snapshot the link before Graph removes it. Later
        // undo support depends on this public record rather than private storage.
        wng::Graph graph;
        const wng::NodeId source = create_node(graph, "Source");
        const wng::NodeId target = create_node(graph, "Target");
        const wng::PortId output = add_port(graph, source, wng::PortKind::Output, "out");
        const wng::PortId input = add_port(graph, target, wng::PortKind::Input, "in");
        const wng::LinkId link = create_link(graph, output, input);

        const wng::GraphCommandResult result = wng::command_destroy_link(graph, link);

        assert(result.result == wng::Result::Ok);
        assert(result.record.kind == wng::GraphCommandKind::DestroyLink);
        assert(result.record.removed_links.size() == 1U);
        assert(result.record.removed_links[0].id == link);
        assert(result.record.removed_links[0].from == output);
        assert(result.record.removed_links[0].to == input);
        assert(graph.find_link(link) == nullptr);
    }

    {
        // Remove-port commands snapshot the port and all connected links before
        // delegating the destructive mutation to Graph.
        wng::Graph graph;
        const wng::NodeId source = create_node(graph, "Source");
        const wng::NodeId target = create_node(graph, "Target");
        const wng::PortId output = add_port(graph, source, wng::PortKind::Output, "out");
        const wng::PortId input = add_port(graph, target, wng::PortKind::Input, "in");
        const wng::LinkId link = create_link(graph, output, input);

        const wng::GraphCommandResult result = wng::command_remove_port(graph, input);

        assert(result.result == wng::Result::Ok);
        assert(result.record.kind == wng::GraphCommandKind::RemovePort);
        assert(result.record.removed_ports.size() == 1U);
        assert(result.record.removed_ports[0].id == input);
        assert(result.record.removed_links.size() == 1U);
        assert(result.record.removed_links[0].id == link);
        assert(graph.find_port(input) == nullptr);
        assert(graph.find_link(link) == nullptr);
        assert(contains_summary_port(result.record.summary, input));
        assert(contains_summary_link(result.record.summary, link));
    }

    {
        // Destroy-node commands snapshot the node, owned ports, and connected
        // links before calling Graph::destroy_node.
        wng::Graph graph;
        const wng::NodeId source = create_node(graph, "Source");
        const wng::NodeId target = create_node(graph, "Target");
        const wng::PortId output = add_port(graph, source, wng::PortKind::Output, "out");
        const wng::PortId input = add_port(graph, target, wng::PortKind::Input, "in");
        const wng::LinkId link = create_link(graph, output, input);

        const wng::GraphCommandResult result = wng::command_destroy_node(graph, source);

        assert(result.result == wng::Result::Ok);
        assert(result.record.kind == wng::GraphCommandKind::DestroyNode);
        assert(result.record.removed_nodes.size() == 1U);
        assert(result.record.removed_nodes[0].id == source);
        assert(result.record.removed_ports.size() == 1U);
        assert(contains_port_id(result.record.removed_ports, output));
        assert(result.record.removed_links.size() == 1U);
        assert(contains_link_id(result.record.removed_links, link));
        assert(graph.find_node(source) == nullptr);
        assert(graph.find_port(output) == nullptr);
        assert(graph.find_link(link) == nullptr);
        assert(contains_summary_port(result.record.summary, output));
        assert(contains_summary_link(result.record.summary, link));
    }

    {
        // Missing-node destruction records failure and avoids claiming undo data
        // exists for a mutation that never occurred.
        wng::Graph graph;
        const wng::NodeId existing = create_node(graph);

        const wng::GraphCommandResult result =
            wng::command_destroy_node(graph, wng::NodeId { 999 });

        assert(result.result == wng::Result::NotFound);
        assert(result.record.kind == wng::GraphCommandKind::DestroyNode);
        assert(result.record.removed_nodes.empty());
        assert(result.record.removed_ports.empty());
        assert(result.record.removed_links.empty());
        assert(graph.find_node(existing) != nullptr);
        assert(graph.nodes().size() == 1U);
    }

    {
        // Command helpers are intentionally schema-free. Unknown schema/domain
        // node types remain valid if the underlying Graph operation allows them.
        wng::Graph graph;
        wng::NodeDesc desc = make_node_desc("Schema Free");
        desc.type = "domain.node.not.registered";

        const wng::GraphCommandResult node_result = wng::command_create_node(graph, desc);
        assert(node_result.result == wng::Result::Ok);

        const wng::PortDesc port_desc = make_port_desc(wng::PortKind::Output, "value");
        const wng::GraphCommandResult port_result =
            wng::command_add_port(graph, node_result.record.node, port_desc);

        assert(port_result.result == wng::Result::Ok);
        assert(graph.find_node(node_result.record.node)->type == desc.type);
        assert(graph.find_port(port_result.record.port) != nullptr);
    }

    return 0;
}
