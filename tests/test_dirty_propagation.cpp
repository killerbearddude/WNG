// Exercises deterministic dirty propagation from changed graph objects.
// These tests protect dependency-analysis semantics without introducing stored
// dirty flags, execution planning, or graph mutation.

#include <cassert>
#include <vector>

#include <wng/dirty_propagation.hpp>
#include <wng/graph.hpp>

namespace
{
    struct NodePorts {
        wng::NodeId node;
        wng::PortId input;
        wng::PortId output;
    };

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

    NodePorts create_node_with_ports(wng::Graph& graph, const char* title)
    {
        NodePorts result;
        result.node = create_node(graph, title);
        result.input = add_port(graph, result.node, wng::PortKind::Input);
        result.output = add_port(graph, result.node, wng::PortKind::Output);
        return result;
    }

    wng::LinkId connect(wng::Graph& graph, wng::PortId from, wng::PortId to)
    {
        wng::LinkId link;
        assert(graph.create_link(from, to, &link) == wng::Result::Ok);
        return link;
    }

    void assert_nodes(const std::vector<wng::NodeId>& actual, const std::vector<wng::NodeId>& expected)
    {
        assert(actual.size() == expected.size());
        for (std::vector<wng::NodeId>::size_type i = 0; i < expected.size(); ++i) {
            assert(actual[i] == expected[i]);
        }
    }

    struct Chain {
        wng::Graph graph;
        NodePorts a;
        NodePorts b;
        NodePorts c;
    };

    Chain make_chain()
    {
        Chain chain;
        chain.a = create_node_with_ports(chain.graph, "A");
        chain.b = create_node_with_ports(chain.graph, "B");
        chain.c = create_node_with_ports(chain.graph, "C");
        connect(chain.graph, chain.a.output, chain.b.input);
        connect(chain.graph, chain.b.output, chain.c.input);
        return chain;
    }
}

int main()
{
    {
        // Empty requests are valid no-op analyses. This protects callers that
        // batch optional change lists and may have nothing to propagate.
        wng::Graph graph;
        const wng::DirtyPropagationRequest request;
        const wng::DirtyPropagationResult result = wng::propagate_dirty(graph, request);

        assert(result.result == wng::Result::Ok);
        assert(result.success());
        assert(result.complete());
        assert(result.source_nodes.empty());
        assert(result.dirty_nodes.empty());
        assert(result.ordered_dirty_nodes.empty());
        assert(result.unresolved_nodes.empty());
    }

    {
        // A changed source node marks itself and every downstream dependency dirty
        // in deterministic discovery and topological order.
        Chain chain = make_chain();

        wng::DirtyPropagationRequest request;
        request.changed_nodes.push_back(chain.a.node);

        const wng::DirtyPropagationResult result = wng::propagate_dirty(chain.graph, request);

        assert(result.success());
        assert_nodes(result.source_nodes, std::vector<wng::NodeId> { chain.a.node });
        assert_nodes(result.dirty_nodes, std::vector<wng::NodeId> { chain.a.node, chain.b.node, chain.c.node });
        assert_nodes(result.ordered_dirty_nodes, std::vector<wng::NodeId> { chain.a.node, chain.b.node, chain.c.node });
    }

    {
        // Excluding sources supports callers that separately handle the edited
        // node and only need affected downstream dependents.
        Chain chain = make_chain();

        wng::DirtyPropagationRequest request;
        request.changed_nodes.push_back(chain.a.node);
        request.include_sources = false;

        const wng::DirtyPropagationResult result = wng::propagate_dirty(chain.graph, request);

        assert(result.success());
        assert_nodes(result.source_nodes, std::vector<wng::NodeId> { chain.a.node });
        assert_nodes(result.dirty_nodes, std::vector<wng::NodeId> { chain.b.node, chain.c.node });
        assert_nodes(result.ordered_dirty_nodes, std::vector<wng::NodeId> { chain.b.node, chain.c.node });
    }

    {
        // Changed ports resolve to their owning node before propagation. This is
        // the common case for pin metadata or connection endpoint changes.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        connect(graph, a.output, b.input);

        wng::DirtyPropagationRequest request;
        request.changed_ports.push_back(a.output);

        const wng::DirtyPropagationResult result = wng::propagate_dirty(graph, request);

        assert(result.success());
        assert_nodes(result.source_nodes, std::vector<wng::NodeId> { a.node });
        assert_nodes(result.dirty_nodes, std::vector<wng::NodeId> { a.node, b.node });
    }

    {
        // Changed links resolve from the producer side so propagation starts at
        // the source/output owner and flows downstream.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const wng::LinkId link = connect(graph, a.output, b.input);

        wng::DirtyPropagationRequest request;
        request.changed_links.push_back(link);

        const wng::DirtyPropagationResult result = wng::propagate_dirty(graph, request);

        assert(result.success());
        assert_nodes(result.source_nodes, std::vector<wng::NodeId> { a.node });
        assert_nodes(result.dirty_nodes, std::vector<wng::NodeId> { a.node, b.node });
    }

    {
        // Multiple changed nodes are deduplicated in request order, and shared
        // downstream dependencies are reported once.
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

        wng::DirtyPropagationRequest request;
        request.changed_nodes.push_back(a.node);
        request.changed_nodes.push_back(b.node);
        request.changed_nodes.push_back(a.node);

        const wng::DirtyPropagationResult result = wng::propagate_dirty(graph, request);

        assert(result.success());
        assert_nodes(result.source_nodes, std::vector<wng::NodeId> { a.node, b.node });
        assert_nodes(result.dirty_nodes, std::vector<wng::NodeId> { a.node, b.node, c.node });
    }

    {
        // Request order controls source reporting. Evaluation/consideration order
        // is computed separately through topological graph order.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const NodePorts c = create_node_with_ports(graph, "C");
        (void)c;

        wng::DirtyPropagationRequest request;
        request.changed_nodes.push_back(b.node);
        request.changed_nodes.push_back(a.node);

        const wng::DirtyPropagationResult result = wng::propagate_dirty(graph, request);

        assert(result.success());
        assert_nodes(result.source_nodes, std::vector<wng::NodeId> { b.node, a.node });
        assert_nodes(result.dirty_nodes, std::vector<wng::NodeId> { b.node, a.node });
        assert_nodes(result.ordered_dirty_nodes, std::vector<wng::NodeId> { a.node, b.node });
    }

    {
        // Zero node IDs are invalid request data and are rejected before lookup.
        wng::Graph graph;
        wng::DirtyPropagationRequest request;
        request.changed_nodes.push_back(wng::NodeId {});

        const wng::DirtyPropagationResult result = wng::propagate_dirty(graph, request);

        assert(result.result == wng::Result::InvalidArgument);
        assert(result.source_nodes.empty());
        assert(result.dirty_nodes.empty());
    }

    {
        // Missing non-zero node IDs are distinct from invalid zero IDs and report
        // NotFound.
        wng::Graph graph;
        wng::DirtyPropagationRequest request;
        request.changed_nodes.push_back(wng::NodeId { 999 });

        const wng::DirtyPropagationResult result = wng::propagate_dirty(graph, request);

        assert(result.result == wng::Result::NotFound);
    }

    {
        // Zero port IDs are rejected before ownership resolution.
        wng::Graph graph;
        wng::DirtyPropagationRequest request;
        request.changed_ports.push_back(wng::PortId {});

        const wng::DirtyPropagationResult result = wng::propagate_dirty(graph, request);

        assert(result.result == wng::Result::InvalidArgument);
    }

    {
        // Missing ports report NotFound and do not create partial source output.
        wng::Graph graph;
        wng::DirtyPropagationRequest request;
        request.changed_ports.push_back(wng::PortId { 999 });

        const wng::DirtyPropagationResult result = wng::propagate_dirty(graph, request);

        assert(result.result == wng::Result::NotFound);
    }

    {
        // Zero link IDs are rejected before endpoint resolution.
        wng::Graph graph;
        wng::DirtyPropagationRequest request;
        request.changed_links.push_back(wng::LinkId {});

        const wng::DirtyPropagationResult result = wng::propagate_dirty(graph, request);

        assert(result.result == wng::Result::InvalidArgument);
    }

    {
        // Missing links report NotFound and do not infer dirty nodes.
        wng::Graph graph;
        wng::DirtyPropagationRequest request;
        request.changed_links.push_back(wng::LinkId { 999 });

        const wng::DirtyPropagationResult result = wng::propagate_dirty(graph, request);

        assert(result.result == wng::Result::NotFound);
    }

    {
        // Cycles remain valid storage, but affected topological ordering is
        // incomplete and reports unresolved dirty nodes in graph storage order.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");

        connect(graph, a.output, b.input);
        connect(graph, b.output, a.input);

        wng::DirtyPropagationRequest request;
        request.changed_nodes.push_back(a.node);

        const wng::DirtyPropagationResult result = wng::propagate_dirty(graph, request);

        assert(result.result == wng::Result::InvalidConnection);
        assert(!result.complete());
        assert_nodes(result.source_nodes, std::vector<wng::NodeId> { a.node });
        assert_nodes(result.dirty_nodes, std::vector<wng::NodeId> { a.node, b.node });
        assert_nodes(result.unresolved_nodes, std::vector<wng::NodeId> { a.node, b.node });
    }

    {
        // Current dirty ordering uses whole-graph topological sort. An unrelated
        // cycle therefore prevents complete ordering even when the changed node is
        // outside that cycle; a future patch may add dirty-subgraph ordering.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const NodePorts c = create_node_with_ports(graph, "C");

        connect(graph, a.output, b.input);
        connect(graph, b.output, a.input);

        wng::DirtyPropagationRequest request;
        request.changed_nodes.push_back(c.node);

        const wng::DirtyPropagationResult result = wng::propagate_dirty(graph, request);

        assert(result.result == wng::Result::InvalidConnection);
        assert_nodes(result.source_nodes, std::vector<wng::NodeId> { c.node });
        assert_nodes(result.dirty_nodes, std::vector<wng::NodeId> { c.node });
        assert(result.unresolved_nodes.empty());
    }

    return 0;
}
