// Exercises deterministic, non-mutating graph traversal helpers.
// The tests intentionally use only public Graph APIs and keep execution planning
// out of scope.

#include <cassert>
#include <vector>

#include <wng/graph.hpp>
#include <wng/graph_traversal.hpp>
#include <wng/graph_validation.hpp>

namespace
{
    wng::NodeId create_node(wng::Graph& graph, const char* title)
    {
        wng::NodeDesc desc;
        desc.title = title;

        wng::NodeId node;
        assert(graph.create_node(desc, &node) == wng::Result::Ok);
        return node;
    }

    wng::PortId add_port(wng::Graph& graph, wng::NodeId node, wng::PortKind kind)
    {
        wng::PortDesc desc;
        desc.kind = kind;
        desc.type = "number";

        wng::PortId port;
        assert(graph.add_port(node, desc, &port) == wng::Result::Ok);
        return port;
    }

    void connect(wng::Graph& graph, wng::PortId from, wng::PortId to)
    {
        wng::LinkId link;
        assert(graph.create_link(from, to, &link) == wng::Result::Ok);
    }

    struct NodePorts {
        wng::NodeId node;
        wng::PortId input;
        wng::PortId output;
    };

    NodePorts create_node_with_ports(wng::Graph& graph, const char* title)
    {
        NodePorts result;
        result.node = create_node(graph, title);
        result.input = add_port(graph, result.node, wng::PortKind::Input);
        result.output = add_port(graph, result.node, wng::PortKind::Output);
        return result;
    }

    void assert_node_sequence(const std::vector<wng::NodeId>& actual, const std::vector<wng::NodeId>& expected)
    {
        assert(actual.size() == expected.size());
        for (std::vector<wng::NodeId>::size_type i = 0; i < expected.size(); ++i) {
            assert(actual[i] == expected[i]);
        }
    }
}

int main()
{
    {
        // Empty graphs have a complete topological order with no nodes.
        wng::Graph graph;
        const wng::TopologicalOrderResult order = wng::topological_sort(graph);

        assert(order.result == wng::Result::Ok);
        assert(order.ordered_nodes.empty());
        assert(order.unresolved_nodes.empty());
        assert(order.complete());
    }

    {
        // A single isolated node appears once in topological order.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "A");

        const wng::TopologicalOrderResult order = wng::topological_sort(graph);

        assert(order.result == wng::Result::Ok);
        assert_node_sequence(order.ordered_nodes, std::vector<wng::NodeId> { node });
        assert(order.unresolved_nodes.empty());
    }

    {
        // Linear dependencies must produce source-before-sink order.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const NodePorts c = create_node_with_ports(graph, "C");

        connect(graph, a.output, b.input);
        connect(graph, b.output, c.input);

        const wng::TopologicalOrderResult order = wng::topological_sort(graph);

        assert(order.result == wng::Result::Ok);
        assert_node_sequence(order.ordered_nodes, std::vector<wng::NodeId> { a.node, b.node, c.node });
        assert(order.complete());
    }

    {
        // Deterministic tie-breaking for independent sources follows node storage
        // order. Future execution planning must not depend on incidental ordering.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const NodePorts c = create_node_with_ports(graph, "C");

        connect(graph, a.output, c.input);

        wng::PortDesc second_input_desc;
        second_input_desc.kind = wng::PortKind::Input;
        second_input_desc.type = "number";
        wng::PortId c_second_input;
        assert(graph.add_port(c.node, second_input_desc, &c_second_input) == wng::Result::Ok);
        connect(graph, b.output, c_second_input);

        const wng::TopologicalOrderResult order = wng::topological_sort(graph);

        assert(order.result == wng::Result::Ok);
        assert_node_sequence(order.ordered_nodes, std::vector<wng::NodeId> { a.node, b.node, c.node });
    }

    {
        // Downstream reachability is breadth-first and scans graph links in storage
        // order: A discovers B and D before expanding B to discover C.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const NodePorts c = create_node_with_ports(graph, "C");
        const NodePorts d = create_node_with_ports(graph, "D");

        connect(graph, a.output, b.input);
        connect(graph, b.output, c.input);

        wng::PortDesc second_output_desc;
        second_output_desc.kind = wng::PortKind::Output;
        second_output_desc.type = "number";
        wng::PortId a_second_output;
        assert(graph.add_port(a.node, second_output_desc, &a_second_output) == wng::Result::Ok);
        connect(graph, a_second_output, d.input);

        const wng::NodeTraversalResult reachable =
            wng::collect_reachable_nodes(graph, a.node, wng::TraversalDirection::Downstream);

        assert(reachable.success());
        assert_node_sequence(reachable.nodes, std::vector<wng::NodeId> { b.node, d.node, c.node });
    }

    {
        // Upstream reachability walks from target input owners back to source output
        // owners. C discovers B first, then expands B to discover A.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const NodePorts c = create_node_with_ports(graph, "C");

        connect(graph, a.output, b.input);
        connect(graph, b.output, c.input);

        const wng::NodeTraversalResult reachable =
            wng::collect_reachable_nodes(graph, c.node, wng::TraversalDirection::Upstream);

        assert(reachable.success());
        assert_node_sequence(reachable.nodes, std::vector<wng::NodeId> { b.node, a.node });
    }

    {
        // The start node is intentionally excluded because callers already know it.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");

        connect(graph, a.output, b.input);

        const wng::NodeTraversalResult reachable =
            wng::collect_reachable_nodes(graph, a.node, wng::TraversalDirection::Downstream);

        assert(reachable.success());
        assert(reachable.nodes.size() == 1U);
        assert(reachable.nodes[0] != a.node);
    }

    {
        // A node reachable by multiple paths is reported only once. This protects
        // traversal clients from duplicate dependency work.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const NodePorts c = create_node_with_ports(graph, "C");

        connect(graph, a.output, c.input);

        wng::PortDesc b_input_desc;
        b_input_desc.kind = wng::PortKind::Input;
        b_input_desc.type = "number";
        wng::PortId b_second_input;
        assert(graph.add_port(b.node, b_input_desc, &b_second_input) == wng::Result::Ok);

        wng::PortDesc c_input_desc;
        c_input_desc.kind = wng::PortKind::Input;
        c_input_desc.type = "number";
        wng::PortId c_second_input;
        assert(graph.add_port(c.node, c_input_desc, &c_second_input) == wng::Result::Ok);

        connect(graph, a.output, b_second_input);
        connect(graph, b.output, c_second_input);

        const wng::NodeTraversalResult reachable =
            wng::collect_reachable_nodes(graph, a.node, wng::TraversalDirection::Downstream);

        assert(reachable.success());
        assert_node_sequence(reachable.nodes, std::vector<wng::NodeId> { c.node, b.node });
    }

    {
        // Invalid start IDs are rejected before validation or traversal.
        wng::Graph graph;
        const wng::NodeTraversalResult reachable =
            wng::collect_reachable_nodes(graph, wng::NodeId {}, wng::TraversalDirection::Downstream);

        assert(reachable.result == wng::Result::InvalidArgument);
        assert(reachable.nodes.empty());
    }

    {
        // Missing start nodes are rejected distinctly from invalid zero IDs.
        wng::Graph graph;
        const wng::NodeTraversalResult reachable =
            wng::collect_reachable_nodes(graph, wng::NodeId { 999 }, wng::TraversalDirection::Downstream);

        assert(reachable.result == wng::Result::NotFound);
        assert(reachable.nodes.empty());
    }

    {
        // Multi-node cycles are valid graph storage for now, but they cannot produce
        // a complete topological order.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");

        connect(graph, a.output, b.input);
        connect(graph, b.output, a.input);

        const wng::TopologicalOrderResult order = wng::topological_sort(graph);

        assert(order.result == wng::Result::InvalidConnection);
        assert(!order.complete());
        assert_node_sequence(order.unresolved_nodes, std::vector<wng::NodeId> { a.node, b.node });
    }

    {
        // Cycle reporting is local to topological sorting. Whole-graph validation
        // remains cycle-policy-neutral until explicit cycle validation is added.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");

        connect(graph, a.output, b.input);
        connect(graph, b.output, a.input);

        assert(wng::validate_graph(graph).valid());
        assert(wng::topological_sort(graph).result == wng::Result::InvalidConnection);
    }

    return 0;
}
