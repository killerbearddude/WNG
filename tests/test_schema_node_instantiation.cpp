// Exercises schema-defined node instantiation as the first multi-object schema
// mutation helper. The tests focus on deterministic port creation order and
// failure atomicity before graph mutation.

#include <cassert>
#include <string>

#include <wng/schema_mutation.hpp>

namespace
{
    wng::PortDefinition make_input(
        const std::string& name,
        const std::string& type = "number")
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Input;
        definition.type = type;
        return definition;
    }

    wng::PortDefinition make_output(
        const std::string& name,
        const std::string& type = "number")
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Output;
        definition.type = type;
        return definition;
    }

    wng::NodeDefinition make_add_definition()
    {
        wng::NodeDefinition definition;
        definition.type = "math.add";
        definition.display_name = "Add";
        definition.inputs.push_back(make_input("a"));
        definition.inputs.push_back(make_input("b"));
        definition.outputs.push_back(make_output("result"));
        return definition;
    }

    wng::GraphSchema make_schema(const wng::NodeDefinition& definition)
    {
        wng::GraphSchema schema;
        assert(schema.add_node_definition(definition) == wng::Result::Ok);
        return schema;
    }

    wng::NodeDesc make_desc(const std::string& type = "math.add")
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = "Add";
        return desc;
    }

    wng::GraphMutationSummary sentinel_summary()
    {
        wng::GraphMutationSummary summary;
        summary.removed_nodes.push_back(wng::NodeId { 91 });
        summary.removed_ports.push_back(wng::PortId { 92 });
        summary.removed_links.push_back(wng::LinkId { 93 });
        return summary;
    }

    void assert_summary_unchanged(const wng::GraphMutationSummary& summary)
    {
        assert(summary.removed_nodes.size() == 1U);
        assert(summary.removed_ports.size() == 1U);
        assert(summary.removed_links.size() == 1U);
        assert(summary.removed_nodes[0] == wng::NodeId { 91 });
        assert(summary.removed_ports[0] == wng::PortId { 92 });
        assert(summary.removed_links[0] == wng::LinkId { 93 });
    }
}

int main()
{
    {
        // Verifies that full node instantiation preserves schema port order.
        // Future editor code can rely on this order for deterministic port display.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema(make_add_definition());
        wng::GraphMutationSummary rollback = sentinel_summary();

        wng::NodeId node;
        assert(wng::instantiate_node(graph, schema, make_desc(), &node, &rollback) ==
            wng::Result::Ok);

        assert(node != wng::NodeId {});
        assert(graph.nodes().size() == 1U);
        assert(graph.nodes()[0].id == node);
        assert(graph.nodes()[0].type == "math.add");
        assert(graph.nodes()[0].title == "Add");
        assert(graph.nodes()[0].inputs.size() == 2U);
        assert(graph.nodes()[0].outputs.size() == 1U);

        const wng::Port* a = graph.find_port(graph.nodes()[0].inputs[0]);
        const wng::Port* b = graph.find_port(graph.nodes()[0].inputs[1]);
        const wng::Port* result = graph.find_port(graph.nodes()[0].outputs[0]);

        assert(a != nullptr);
        assert(a->kind == wng::PortKind::Input);
        assert(a->name == "a");
        assert(a->type == "number");
        assert(a->visible);
        assert(a->enabled);

        assert(b != nullptr);
        assert(b->kind == wng::PortKind::Input);
        assert(b->name == "b");
        assert(b->type == "number");

        assert(result != nullptr);
        assert(result->kind == wng::PortKind::Output);
        assert(result->name == "result");
        assert(result->type == "number");
        assert_summary_unchanged(rollback);
    }

    {
        // Null node output pointers fail before any schema lookup or mutation,
        // preserving both graph state and rollback summary state.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema(make_add_definition());
        wng::GraphMutationSummary rollback = sentinel_summary();

        assert(wng::instantiate_node(graph, schema, make_desc(), nullptr, &rollback) ==
            wng::Result::InvalidArgument);

        assert(graph.nodes().empty());
        assert(graph.ports().empty());
        assert_summary_unchanged(rollback);
    }

    {
        // Empty node type cannot be resolved to a schema definition, so the helper
        // rejects it without publishing outputs or touching the graph.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema(make_add_definition());
        wng::GraphMutationSummary rollback = sentinel_summary();

        wng::NodeDesc desc = make_desc();
        desc.type.clear();

        wng::NodeId node { 10 };
        assert(wng::instantiate_node(graph, schema, desc, &node, &rollback) ==
            wng::Result::InvalidArgument);

        assert(node == wng::NodeId { 10 });
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
        assert_summary_unchanged(rollback);
    }

    {
        // Unknown node types fail before mutation because no NodeDefinition can
        // authorize which concrete node and ports should be created.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema(make_add_definition());

        wng::NodeId node { 11 };
        assert(wng::instantiate_node(graph, schema, make_desc("missing.node"), &node, nullptr) ==
            wng::Result::NotFound);

        assert(node == wng::NodeId { 11 });
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
    }

    {
        // Disabled node definitions reject at schema-policy level before the
        // graph allocates any IDs.
        wng::Graph graph;
        wng::NodeDefinition definition = make_add_definition();
        definition.enabled = false;
        const wng::GraphSchema schema = make_schema(definition);

        wng::NodeId node { 12 };
        assert(wng::instantiate_node(graph, schema, make_desc(), &node, nullptr) ==
            wng::Result::InvalidConnection);

        assert(node == wng::NodeId { 12 });
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
    }

    {
        // Disabled port definitions are prevalidated. That prevents a partial
        // node from being created just to discover that a port is not instantiable.
        wng::Graph graph;
        wng::NodeDefinition definition = make_add_definition();
        definition.inputs[0].enabled = false;
        const wng::GraphSchema schema = make_schema(definition);
        wng::GraphMutationSummary rollback = sentinel_summary();

        wng::NodeId node { 13 };
        assert(wng::instantiate_node(graph, schema, make_desc(), &node, &rollback) ==
            wng::Result::InvalidConnection);

        assert(node == wng::NodeId { 13 });
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
        assert_summary_unchanged(rollback);
    }

    {
        // Invalid input direction is rejected by GraphSchema registration before
        // an instantiable schema can exist, so Graph remains untouched.
        wng::Graph graph;
        wng::GraphSchema schema;
        wng::NodeDefinition definition = make_add_definition();
        definition.inputs[0].kind = wng::PortKind::Output;

        assert(schema.add_node_definition(definition) == wng::Result::InvalidArgument);
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
    }

    {
        // Invalid output direction is rejected by GraphSchema registration for
        // the same reason as invalid input direction.
        wng::Graph graph;
        wng::GraphSchema schema;
        wng::NodeDefinition definition = make_add_definition();
        definition.outputs[0].kind = wng::PortKind::Input;

        assert(schema.add_node_definition(definition) == wng::Result::InvalidArgument);
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
    }

    {
        // Duplicate input names would make deterministic schema expansion
        // ambiguous, so GraphSchema rejects the definition before instantiation.
        wng::Graph graph;
        wng::GraphSchema schema;
        wng::NodeDefinition definition = make_add_definition();
        definition.inputs[1].name = definition.inputs[0].name;

        assert(schema.add_node_definition(definition) == wng::Result::AlreadyExists);
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
    }

    {
        // Duplicate output names are rejected independently from input names so
        // output port identity remains deterministic.
        wng::Graph graph;
        wng::GraphSchema schema;
        wng::NodeDefinition definition = make_add_definition();
        definition.outputs.push_back(make_output("result"));

        assert(schema.add_node_definition(definition) == wng::Result::AlreadyExists);
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
    }

    {
        // Visibility is editor/rendering state, not validation state. Instantiation
        // preserves visibility values while requiring enabled definitions.
        wng::Graph graph;
        wng::NodeDefinition definition = make_add_definition();
        definition.visible = false;
        definition.inputs[0].visible = false;
        definition.outputs[0].visible = false;
        const wng::GraphSchema schema = make_schema(definition);

        wng::NodeDesc desc = make_desc();
        desc.visible = false;

        wng::NodeId node;
        assert(wng::instantiate_node(graph, schema, desc, &node, nullptr) == wng::Result::Ok);

        assert(graph.nodes()[0].visible == false);
        const wng::Port* input = graph.find_port(graph.nodes()[0].inputs[0]);
        const wng::Port* output = graph.find_port(graph.nodes()[0].outputs[0]);
        assert(input != nullptr);
        assert(output != nullptr);
        assert(input->visible == false);
        assert(output->visible == false);
    }

    {
        // Display title defaults remain caller/editor policy. The graph core helper
        // does not auto-fill an empty title from NodeDefinition::display_name.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema(make_add_definition());

        wng::NodeDesc desc = make_desc();
        desc.title.clear();

        wng::NodeId node;
        assert(wng::instantiate_node(graph, schema, desc, &node, nullptr) == wng::Result::Ok);
        assert(graph.nodes()[0].title.empty());
    }

    {
        // Existing schema-aware create_node remains a single-object helper. Full
        // port expansion is isolated to instantiate_node.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema(make_add_definition());

        wng::NodeId node;
        assert(wng::create_node(graph, schema, make_desc(), &node) == wng::Result::Ok);

        assert(graph.nodes().size() == 1U);
        assert(graph.nodes()[0].inputs.empty());
        assert(graph.nodes()[0].outputs.empty());
        assert(graph.ports().empty());
    }

    return 0;
}
