// Exercises deterministic schema validation issue ordering.
// Schema diagnostics must preserve node traversal and graph-owned port order so
// editor and regression tooling can produce stable messages.

#include <cassert>
#include <string>

#include <wng/graph_validation.hpp>
#include <wng/schema.hpp>

namespace
{
    wng::NodeDefinition node_definition(const std::string& type)
    {
        wng::NodeDefinition definition;
        definition.type = type;
        definition.display_name = type;
        return definition;
    }

    wng::GraphSchema make_schema()
    {
        wng::GraphSchema schema;
        assert(schema.add_node_definition(node_definition("test.node")) ==
            wng::Result::Ok);
        return schema;
    }

    wng::NodeId create_node(wng::Graph& graph, const std::string& title)
    {
        wng::NodeDesc desc;
        desc.type = "test.node";
        desc.title = title;

        wng::NodeId id;
        assert(graph.create_node(desc, &id) == wng::Result::Ok);
        return id;
    }

    wng::PortId add_input(
        wng::Graph& graph,
        wng::NodeId node,
        const std::string& name)
    {
        wng::PortDesc desc;
        desc.kind = wng::PortKind::Input;
        desc.name = name;
        desc.type = "number";

        wng::PortId id;
        assert(graph.add_port(node, desc, &id) == wng::Result::Ok);
        return id;
    }
}

int main()
{
    // The schema defines the node type but no ports. Every graph-owned port is an
    // undeclared-port diagnostic. Validation walks nodes in graph order and, for
    // each node, scans graph port storage order to keep report ordering stable.
    wng::Graph graph;
    const wng::NodeId first_node = create_node(graph, "First");
    const wng::NodeId second_node = create_node(graph, "Second");
    const wng::PortId first_a = add_input(graph, first_node, "first_a");
    const wng::PortId second_a = add_input(graph, second_node, "second_a");
    const wng::PortId first_b = add_input(graph, first_node, "first_b");

    const wng::ValidationReport report = wng::validate_graph(graph, make_schema());

    assert(!report.valid());
    assert(report.issues.size() == 3U);
    assert(report.issues[0].code == wng::ValidationIssueCode::MissingPortDefinition);
    assert(report.issues[0].node == first_node);
    assert(report.issues[0].port == first_a);
    assert(report.issues[1].code == wng::ValidationIssueCode::MissingPortDefinition);
    assert(report.issues[1].node == first_node);
    assert(report.issues[1].port == first_b);
    assert(report.issues[2].code == wng::ValidationIssueCode::MissingPortDefinition);
    assert(report.issues[2].node == second_node);
    assert(report.issues[2].port == second_a);

    return 0;
}
