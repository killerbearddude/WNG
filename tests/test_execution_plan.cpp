// Exercises deterministic, non-executing execution plan construction.
// These tests protect plan metadata semantics without introducing evaluators,
// runtime values, callbacks, or graph mutation.

#include <cassert>
#include <vector>

#include <wng/execution_plan.hpp>
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

    const wng::ExecutionPlanStep& step_for(const wng::ExecutionPlan& plan, wng::NodeId node)
    {
        for (const wng::ExecutionPlanStep& step : plan.steps) {
            if (step.node == node) {
                return step;
            }
        }

        assert(false);
        return plan.steps[0];
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
        // Empty whole-graph planning succeeds and produces no executable work.
        wng::Graph graph;
        wng::ExecutionPlanRequest request;
        request.scope = wng::ExecutionPlanScope::WholeGraph;

        const wng::ExecutionPlan plan = wng::build_execution_plan(graph, request);

        assert(plan.result == wng::Result::Ok);
        assert(plan.success());
        assert(plan.complete());
        assert(plan.source_nodes.empty());
        assert(plan.planned_nodes.empty());
        assert(plan.unresolved_nodes.empty());
        assert(plan.steps.empty());
    }

    {
        // A single node has no graph dependencies or dependents, but still appears
        // as one deterministic planning step.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "A");

        const wng::ExecutionPlan plan = wng::build_execution_plan(graph, wng::ExecutionPlanRequest {});

        assert(plan.success());
        assert_nodes(plan.planned_nodes, std::vector<wng::NodeId> { a });
        assert(plan.steps.size() == 1U);
        assert(plan.steps[0].node == a);
        assert(plan.steps[0].dependencies.empty());
        assert(plan.steps[0].dependents.empty());
    }

    {
        // Linear whole-graph planning describes source-before-sink order and the
        // immediate dependency relationships future evaluators can consume.
        Chain chain = make_chain();

        const wng::ExecutionPlan plan =
            wng::build_execution_plan(chain.graph, wng::ExecutionPlanRequest {});

        assert(plan.success());
        assert_nodes(plan.planned_nodes, std::vector<wng::NodeId> {
            chain.a.node,
            chain.b.node,
            chain.c.node
        });

        assert_nodes(step_for(plan, chain.a.node).dependencies, std::vector<wng::NodeId> {});
        assert_nodes(step_for(plan, chain.a.node).dependents, std::vector<wng::NodeId> { chain.b.node });

        assert_nodes(step_for(plan, chain.b.node).dependencies, std::vector<wng::NodeId> { chain.a.node });
        assert_nodes(step_for(plan, chain.b.node).dependents, std::vector<wng::NodeId> { chain.c.node });

        assert_nodes(step_for(plan, chain.c.node).dependencies, std::vector<wng::NodeId> { chain.b.node });
        assert_nodes(step_for(plan, chain.c.node).dependents, std::vector<wng::NodeId> {});
    }

    {
        // Branching dependencies are collected in graph link storage order. This
        // protects deterministic metadata for nodes with multiple producers.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const NodePorts c = create_node_with_ports(graph, "C");
        const NodePorts d = create_node_with_ports(graph, "D");

        connect(graph, a.output, c.input);

        wng::PortDesc c_second_input_desc;
        c_second_input_desc.kind = wng::PortKind::Input;
        c_second_input_desc.type = "number";
        wng::PortId c_second_input;
        assert(graph.add_port(c.node, c_second_input_desc, &c_second_input) == wng::Result::Ok);
        connect(graph, b.output, c_second_input);
        connect(graph, c.output, d.input);

        const wng::ExecutionPlan plan =
            wng::build_execution_plan(graph, wng::ExecutionPlanRequest {});

        assert(plan.success());
        assert_nodes(plan.planned_nodes, std::vector<wng::NodeId> {
            a.node,
            b.node,
            c.node,
            d.node
        });
        assert_nodes(step_for(plan, c.node).dependencies, std::vector<wng::NodeId> {
            a.node,
            b.node
        });
    }

    {
        // Dirty-subgraph planning reuses dirty propagation. A change to B plans B
        // and downstream C, but excludes unrelated D.
        Chain chain = make_chain();
        const NodePorts d = create_node_with_ports(chain.graph, "D");
        (void)d;

        wng::ExecutionPlanRequest request;
        request.scope = wng::ExecutionPlanScope::DirtySubgraph;
        request.changed_nodes.push_back(chain.b.node);
        request.include_dirty_sources = true;

        const wng::ExecutionPlan plan = wng::build_execution_plan(chain.graph, request);

        assert(plan.success());
        assert_nodes(plan.source_nodes, std::vector<wng::NodeId> { chain.b.node });
        assert_nodes(plan.planned_nodes, std::vector<wng::NodeId> { chain.b.node, chain.c.node });
        assert(plan.steps.size() == 2U);
        assert_nodes(step_for(plan, chain.b.node).dependencies, std::vector<wng::NodeId> {});
        assert_nodes(step_for(plan, chain.b.node).dependents, std::vector<wng::NodeId> { chain.c.node });
    }

    {
        // Dirty planning can exclude the edited source node when the caller
        // handles the source separately and wants only downstream work.
        Chain chain = make_chain();

        wng::ExecutionPlanRequest request;
        request.scope = wng::ExecutionPlanScope::DirtySubgraph;
        request.changed_nodes.push_back(chain.b.node);
        request.include_dirty_sources = false;

        const wng::ExecutionPlan plan = wng::build_execution_plan(chain.graph, request);

        assert(plan.success());
        assert_nodes(plan.source_nodes, std::vector<wng::NodeId> { chain.b.node });
        assert_nodes(plan.planned_nodes, std::vector<wng::NodeId> { chain.c.node });
        assert(plan.steps.size() == 1U);
        assert(step_for(plan, chain.c.node).dependencies.empty());
    }

    {
        // Changed ports become dirty sources through ownership resolution, then
        // planning annotates the resulting affected node order.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        connect(graph, a.output, b.input);

        wng::ExecutionPlanRequest request;
        request.scope = wng::ExecutionPlanScope::DirtySubgraph;
        request.changed_ports.push_back(a.output);

        const wng::ExecutionPlan plan = wng::build_execution_plan(graph, request);

        assert(plan.success());
        assert_nodes(plan.source_nodes, std::vector<wng::NodeId> { a.node });
        assert_nodes(plan.planned_nodes, std::vector<wng::NodeId> { a.node, b.node });
    }

    {
        // Changed links become dirty sources from the producer side, matching
        // dirty propagation policy and avoiding execution-specific semantics.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const wng::LinkId link = connect(graph, a.output, b.input);

        wng::ExecutionPlanRequest request;
        request.scope = wng::ExecutionPlanScope::DirtySubgraph;
        request.changed_links.push_back(link);

        const wng::ExecutionPlan plan = wng::build_execution_plan(graph, request);

        assert(plan.success());
        assert_nodes(plan.source_nodes, std::vector<wng::NodeId> { a.node });
        assert_nodes(plan.planned_nodes, std::vector<wng::NodeId> { a.node, b.node });
    }

    {
        // Whole-graph scope ignores dirty vectors by contract. The selected scope,
        // not incidental request fields, controls planning behavior.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        connect(graph, a.output, b.input);

        wng::ExecutionPlanRequest request;
        request.scope = wng::ExecutionPlanScope::WholeGraph;
        request.changed_nodes.push_back(b.node);

        const wng::ExecutionPlan plan = wng::build_execution_plan(graph, request);

        assert(plan.success());
        assert(plan.source_nodes.empty());
        assert_nodes(plan.planned_nodes, std::vector<wng::NodeId> { a.node, b.node });
    }

    {
        // Dirty-subgraph request errors propagate from dirty propagation and do
        // not produce partial planning steps.
        wng::Graph graph;

        wng::ExecutionPlanRequest request;
        request.scope = wng::ExecutionPlanScope::DirtySubgraph;
        request.changed_nodes.push_back(wng::NodeId { 999 });

        const wng::ExecutionPlan plan = wng::build_execution_plan(graph, request);

        assert(plan.result == wng::Result::NotFound);
        assert(plan.steps.empty());
    }

    {
        // Cycles prevent whole-graph planning from producing a complete order and
        // are reported through unresolved nodes rather than graph mutation.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");

        connect(graph, a.output, b.input);
        connect(graph, b.output, a.input);

        const wng::ExecutionPlan plan =
            wng::build_execution_plan(graph, wng::ExecutionPlanRequest {});

        assert(plan.result == wng::Result::InvalidConnection);
        assert(!plan.complete());
        assert_nodes(plan.unresolved_nodes, std::vector<wng::NodeId> { a.node, b.node });
    }

    {
        // Dirty-subgraph planning inherits dirty propagation cycle behavior and
        // reports unresolved dirty nodes without adding evaluator semantics.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");

        connect(graph, a.output, b.input);
        connect(graph, b.output, a.input);

        wng::ExecutionPlanRequest request;
        request.scope = wng::ExecutionPlanScope::DirtySubgraph;
        request.changed_nodes.push_back(a.node);

        const wng::ExecutionPlan plan = wng::build_execution_plan(graph, request);

        assert(plan.result == wng::Result::InvalidConnection);
        assert(!plan.complete());
        assert_nodes(plan.source_nodes, std::vector<wng::NodeId> { a.node });
        assert_nodes(plan.unresolved_nodes, std::vector<wng::NodeId> { a.node, b.node });
    }

    {
        // Planning must be non-mutating. Both whole-graph and dirty-subgraph plans
        // leave node, port, link counts and IDs unchanged.
        wng::Graph graph;
        const NodePorts a = create_node_with_ports(graph, "A");
        const NodePorts b = create_node_with_ports(graph, "B");
        const wng::LinkId link = connect(graph, a.output, b.input);

        const std::vector<wng::Node> nodes_before = graph.nodes();
        const std::vector<wng::Port> ports_before = graph.ports();
        const std::vector<wng::Link> links_before = graph.links();

        wng::ExecutionPlanRequest whole_request;
        whole_request.scope = wng::ExecutionPlanScope::WholeGraph;
        assert(wng::build_execution_plan(graph, whole_request).success());

        wng::ExecutionPlanRequest dirty_request;
        dirty_request.scope = wng::ExecutionPlanScope::DirtySubgraph;
        dirty_request.changed_links.push_back(link);
        assert(wng::build_execution_plan(graph, dirty_request).success());

        assert(graph.nodes().size() == nodes_before.size());
        assert(graph.ports().size() == ports_before.size());
        assert(graph.links().size() == links_before.size());

        assert(graph.nodes()[0].id == nodes_before[0].id);
        assert(graph.nodes()[1].id == nodes_before[1].id);
        assert(graph.ports()[0].id == ports_before[0].id);
        assert(graph.links()[0].id == links_before[0].id);
    }

    return 0;
}
