// Exercises non-mutating previews for destructive graph mutations.
// These tests lock preview output to actual GraphMutationSummary behavior so
// future editor consequence panels can trust previews before applying changes.

#include <cassert>
#include <vector>

#include <wng/graph.hpp>
#include <wng/graph_mutation_preview.hpp>
#include <wng/serialization.hpp>

namespace
{
    struct NodePorts {
        wng::NodeId node;
        wng::PortId input;
        wng::PortId output;
    };

    wng::NodeDesc node_desc(const char* title)
    {
        wng::NodeDesc desc;
        desc.type = "test.node";
        desc.title = title;
        return desc;
    }

    wng::NodeId create_node(wng::Graph& graph, const char* title)
    {
        wng::NodeId node;
        assert(graph.create_node(node_desc(title), &node) == wng::Result::Ok);
        return node;
    }

    wng::PortDesc port_desc(
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
        const char* name,
        const char* type = "number")
    {
        wng::PortId port;
        assert(graph.add_port(node, port_desc(kind, name, type), &port) == wng::Result::Ok);
        return port;
    }

    wng::LinkId create_link(
        wng::Graph& graph,
        wng::PortId from,
        wng::PortId to)
    {
        wng::LinkId link;
        assert(graph.create_link(from, to, &link) == wng::Result::Ok);
        return link;
    }

    NodePorts create_node_with_ports(wng::Graph& graph, const char* title)
    {
        NodePorts result;
        result.node = create_node(graph, title);
        result.input = add_port(graph, result.node, wng::PortKind::Input, "in");
        result.output = add_port(graph, result.node, wng::PortKind::Output, "out");
        return result;
    }

    wng::Graph copy_graph(const wng::Graph& graph)
    {
        wng::GraphDto dto;
        assert(wng::export_graph(graph, &dto) == wng::Result::Ok);

        wng::Graph copy;
        assert(wng::import_graph(dto, &copy) == wng::Result::Ok);
        return copy;
    }

    void assert_ids(
        const std::vector<wng::NodeId>& actual,
        const std::vector<wng::NodeId>& expected)
    {
        assert(actual.size() == expected.size());
        for (std::vector<wng::NodeId>::size_type i = 0; i < expected.size(); ++i) {
            assert(actual[i] == expected[i]);
        }
    }

    void assert_ids(
        const std::vector<wng::PortId>& actual,
        const std::vector<wng::PortId>& expected)
    {
        assert(actual.size() == expected.size());
        for (std::vector<wng::PortId>::size_type i = 0; i < expected.size(); ++i) {
            assert(actual[i] == expected[i]);
        }
    }

    void assert_ids(
        const std::vector<wng::LinkId>& actual,
        const std::vector<wng::LinkId>& expected)
    {
        assert(actual.size() == expected.size());
        for (std::vector<wng::LinkId>::size_type i = 0; i < expected.size(); ++i) {
            assert(actual[i] == expected[i]);
        }
    }

    void assert_summary_equals(
        const wng::GraphMutationSummary& actual,
        const wng::GraphMutationSummary& expected)
    {
        assert_ids(actual.removed_nodes, expected.removed_nodes);
        assert_ids(actual.removed_ports, expected.removed_ports);
        assert_ids(actual.removed_links, expected.removed_links);
    }

    struct GraphState {
        std::vector<wng::NodeId> nodes;
        std::vector<wng::PortId> ports;
        std::vector<wng::LinkId> links;
    };

    GraphState capture_state(const wng::Graph& graph)
    {
        GraphState state;
        for (const wng::Node& node : graph.nodes()) {
            state.nodes.push_back(node.id);
        }
        for (const wng::Port& port : graph.ports()) {
            state.ports.push_back(port.id);
        }
        for (const wng::Link& link : graph.links()) {
            state.links.push_back(link.id);
        }
        return state;
    }

    void assert_state_unchanged(const wng::Graph& graph, const GraphState& state)
    {
        assert_ids(capture_state(graph).nodes, state.nodes);
        assert_ids(capture_state(graph).ports, state.ports);
        assert_ids(capture_state(graph).links, state.links);
    }

    struct Chain {
        wng::Graph graph;
        NodePorts a;
        NodePorts b;
        NodePorts c;
        wng::LinkId a_to_b;
        wng::LinkId b_to_c;
    };

    Chain make_chain()
    {
        Chain chain;
        chain.a = create_node_with_ports(chain.graph, "A");
        chain.b = create_node_with_ports(chain.graph, "B");
        chain.c = create_node_with_ports(chain.graph, "C");
        chain.a_to_b = create_link(chain.graph, chain.a.output, chain.b.input);
        chain.b_to_c = create_link(chain.graph, chain.b.output, chain.c.input);
        return chain;
    }

    void assert_preview_empty(const wng::GraphMutationPreview& preview)
    {
        assert(preview.summary.removed_nodes.empty());
        assert(preview.summary.removed_ports.empty());
        assert(preview.summary.removed_links.empty());
    }
}

int main()
{
    {
        // Rejects zero node IDs without touching graph state. Future callers can
        // safely ask for previews before validating selection contents.
        wng::Graph graph;
        const GraphState before = capture_state(graph);

        const wng::GraphMutationPreview preview =
            wng::preview_destroy_node(graph, wng::NodeId {});

        assert(preview.result == wng::Result::InvalidArgument);
        assert_preview_empty(preview);
        assert_state_unchanged(graph, before);
    }

    {
        // Missing nodes are reported as NotFound and do not produce speculative
        // cleanup data that an editor could accidentally present as real.
        wng::Graph graph;
        const GraphState before = capture_state(graph);

        const wng::GraphMutationPreview preview =
            wng::preview_destroy_node(graph, wng::NodeId { 999 });

        assert(preview.result == wng::Result::NotFound);
        assert_preview_empty(preview);
        assert_state_unchanged(graph, before);
    }

    {
        // Destroy-node preview reports the target node, owned ports, and links in
        // graph link storage order without modifying the graph.
        Chain chain = make_chain();
        const GraphState before = capture_state(chain.graph);

        const wng::GraphMutationPreview preview =
            wng::preview_destroy_node(chain.graph, chain.b.node);

        assert(preview.result == wng::Result::Ok);
        assert(!preview.empty());
        assert_ids(preview.summary.removed_nodes, std::vector<wng::NodeId> { chain.b.node });
        assert_ids(preview.summary.removed_ports, std::vector<wng::PortId> {
            chain.b.input,
            chain.b.output
        });
        assert_ids(preview.summary.removed_links, std::vector<wng::LinkId> {
            chain.a_to_b,
            chain.b_to_c
        });
        assert_state_unchanged(chain.graph, before);
    }

    {
        // Locks preview ordering to actual destroy-node mutation summary ordering.
        // Future mutation-summary changes must update preview semantics together.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "Node");
        const wng::PortId in_a = add_port(graph, node, wng::PortKind::Input, "in-a");
        const wng::PortId in_b = add_port(graph, node, wng::PortKind::Input, "in-b");
        const wng::PortId out_a = add_port(graph, node, wng::PortKind::Output, "out-a");
        const wng::PortId out_b = add_port(graph, node, wng::PortKind::Output, "out-b");
        (void)in_a;
        (void)in_b;
        (void)out_a;
        (void)out_b;

        const wng::GraphMutationPreview preview = wng::preview_destroy_node(graph, node);

        wng::Graph actual_graph = copy_graph(graph);
        wng::GraphMutationSummary actual_summary;
        assert(actual_graph.destroy_node(node, &actual_summary) == wng::Result::Ok);

        assert_summary_equals(preview.summary, actual_summary);
    }

    {
        // Rejects zero port IDs without mutating graph state.
        wng::Graph graph;
        const GraphState before = capture_state(graph);

        const wng::GraphMutationPreview preview =
            wng::preview_remove_port(graph, wng::PortId {});

        assert(preview.result == wng::Result::InvalidArgument);
        assert_preview_empty(preview);
        assert_state_unchanged(graph, before);
    }

    {
        // Missing ports return NotFound and leave the graph untouched.
        wng::Graph graph;
        const GraphState before = capture_state(graph);

        const wng::GraphMutationPreview preview =
            wng::preview_remove_port(graph, wng::PortId { 999 });

        assert(preview.result == wng::Result::NotFound);
        assert_preview_empty(preview);
        assert_state_unchanged(graph, before);
    }

    {
        // Remove-port preview reports the port plus dependent links so future
        // transaction planning can show the exact link cleanup before mutation.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const wng::LinkId link = create_link(graph, a.output, b.input);
        const GraphState before = capture_state(graph);

        const wng::GraphMutationPreview preview =
            wng::preview_remove_port(graph, b.input);

        assert(preview.result == wng::Result::Ok);
        assert(preview.summary.removed_nodes.empty());
        assert_ids(preview.summary.removed_ports, std::vector<wng::PortId> { b.input });
        assert_ids(preview.summary.removed_links, std::vector<wng::LinkId> { link });
        assert_state_unchanged(graph, before);
    }

    {
        // Locks remove-port preview ordering to actual mutation-summary ordering
        // when several links are cleaned up from one output port.
        wng::Graph graph;
        const NodePorts source = create_node_with_ports(graph, "Source");
        const NodePorts target_a = create_node_with_ports(graph, "Target A");
        const NodePorts target_b = create_node_with_ports(graph, "Target B");
        create_link(graph, source.output, target_a.input);
        create_link(graph, source.output, target_b.input);

        const wng::GraphMutationPreview preview =
            wng::preview_remove_port(graph, source.output);

        wng::Graph actual_graph = copy_graph(graph);
        wng::GraphMutationSummary actual_summary;
        assert(actual_graph.remove_port(source.output, &actual_summary) == wng::Result::Ok);

        assert_summary_equals(preview.summary, actual_summary);
    }

    {
        // Rejects zero link IDs without graph mutation.
        wng::Graph graph;
        const GraphState before = capture_state(graph);

        const wng::GraphMutationPreview preview =
            wng::preview_destroy_link(graph, wng::LinkId {});

        assert(preview.result == wng::Result::InvalidArgument);
        assert_preview_empty(preview);
        assert_state_unchanged(graph, before);
    }

    {
        // Missing links return NotFound without speculative removal data.
        wng::Graph graph;
        const GraphState before = capture_state(graph);

        const wng::GraphMutationPreview preview =
            wng::preview_destroy_link(graph, wng::LinkId { 999 });

        assert(preview.result == wng::Result::NotFound);
        assert_preview_empty(preview);
        assert_state_unchanged(graph, before);
    }

    {
        // Destroy-link preview reports exactly one link and preserves all nodes
        // and ports. This is the minimal destructive preview case.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const wng::LinkId link = create_link(graph, a.output, b.input);
        const GraphState before = capture_state(graph);

        const wng::GraphMutationPreview preview =
            wng::preview_destroy_link(graph, link);

        assert(preview.result == wng::Result::Ok);
        assert(preview.summary.removed_nodes.empty());
        assert(preview.summary.removed_ports.empty());
        assert_ids(preview.summary.removed_links, std::vector<wng::LinkId> { link });
        assert_state_unchanged(graph, before);
    }

    {
        // Locks destroy-link preview to actual destroy-link summary behavior.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const wng::LinkId link = create_link(graph, a.output, b.input);

        const wng::GraphMutationPreview preview =
            wng::preview_destroy_link(graph, link);

        wng::Graph actual_graph = copy_graph(graph);
        wng::GraphMutationSummary actual_summary;
        assert(actual_graph.destroy_link(link, &actual_summary) == wng::Result::Ok);

        assert_summary_equals(preview.summary, actual_summary);
    }

    {
        // Verifies preview is not secretly consuming stable IDs. Creating the
        // same objects after previews should produce the same IDs as no preview.
        wng::Graph previewed;
        const NodePorts preview_a = create_node_with_ports(previewed, "A");
        const NodePorts preview_b = create_node_with_ports(previewed, "B");
        const wng::LinkId preview_link = create_link(previewed, preview_a.output, preview_b.input);

        wng::Graph control = copy_graph(previewed);

        assert(wng::preview_destroy_node(previewed, preview_a.node).result == wng::Result::Ok);
        assert(wng::preview_remove_port(previewed, preview_b.input).result == wng::Result::Ok);
        assert(wng::preview_destroy_link(previewed, preview_link).result == wng::Result::Ok);

        const wng::NodeId preview_node = create_node(previewed, "New");
        const wng::NodeId control_node = create_node(control, "New");
        assert(preview_node == control_node);

        const wng::PortId preview_output =
            add_port(previewed, preview_node, wng::PortKind::Output, "out");
        const wng::PortId control_output =
            add_port(control, control_node, wng::PortKind::Output, "out");
        assert(preview_output == control_output);

        const wng::PortId preview_input =
            add_port(previewed, preview_b.node, wng::PortKind::Input, "new-in");
        const wng::PortId control_input =
            add_port(control, preview_b.node, wng::PortKind::Input, "new-in");
        assert(preview_input == control_input);

        const wng::LinkId preview_new_link = create_link(previewed, preview_output, preview_input);
        const wng::LinkId control_new_link = create_link(control, control_output, control_input);
        assert(preview_new_link == control_new_link);
    }

    {
        // Empty graphs fail predictably with NotFound for non-zero IDs. This
        // gives callers a stable distinction between invalid zero IDs and absent IDs.
        wng::Graph graph;

        assert(wng::preview_destroy_node(graph, wng::NodeId { 1 }).result == wng::Result::NotFound);
        assert(wng::preview_remove_port(graph, wng::PortId { 1 }).result == wng::Result::NotFound);
        assert(wng::preview_destroy_link(graph, wng::LinkId { 1 }).result == wng::Result::NotFound);
    }

    {
        // Disabled objects still have structural removal consequences. Enabled
        // state affects creation/validation policies, not destructive previews.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const wng::LinkId link = create_link(graph, a.output, b.input);

        assert(graph.find_node(b.node) != nullptr);
        assert(graph.find_port(b.input) != nullptr);
        assert(graph.find_link(link) != nullptr);
        graph.find_node(b.node)->enabled = false;
        graph.find_port(b.input)->enabled = false;
        graph.find_link(link)->enabled = false;

        assert(wng::preview_destroy_node(graph, b.node).result == wng::Result::Ok);
        assert(wng::preview_remove_port(graph, b.input).result == wng::Result::Ok);
        assert(wng::preview_destroy_link(graph, link).result == wng::Result::Ok);
        assert(graph.find_node(b.node) != nullptr);
        assert(graph.find_port(b.input) != nullptr);
        assert(graph.find_link(link) != nullptr);
    }

    return 0;
}
