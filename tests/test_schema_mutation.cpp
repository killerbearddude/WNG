// Exercises the opt-in schema-aware mutation layer.
// These tests protect the boundary where schema policy can restrict link
// creation without making Graph own or depend on GraphSchema.

#include <cassert>
#include <string>

#include <wng/schema_mutation.hpp>

namespace
{
    struct Fixture {
        wng::Graph graph;
        wng::GraphSchema schema;
        wng::NodeId source;
        wng::NodeId target;
        wng::PortId output;
        wng::PortId input;
    };

    wng::PortDefinition output_definition(const std::string& type = "number")
    {
        wng::PortDefinition definition;
        definition.name = "value";
        definition.kind = wng::PortKind::Output;
        definition.type = type;
        return definition;
    }

    wng::PortDefinition input_definition(const std::string& type = "number")
    {
        wng::PortDefinition definition;
        definition.name = "value";
        definition.kind = wng::PortKind::Input;
        definition.type = type;
        return definition;
    }

    wng::NodeDefinition source_definition(const std::string& type = "number")
    {
        wng::NodeDefinition definition;
        definition.type = "constant.number";
        definition.display_name = "Number Constant";
        definition.outputs.push_back(output_definition(type));
        return definition;
    }

    wng::NodeDefinition target_definition(const std::string& type = "number")
    {
        wng::NodeDefinition definition;
        definition.type = "debug.print";
        definition.display_name = "Debug Print";
        definition.inputs.push_back(input_definition(type));
        return definition;
    }

    wng::NodeId create_node(
        wng::Graph& graph,
        const std::string& type,
        const std::string& title)
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = title;

        wng::NodeId id;
        assert(graph.create_node(desc, &id) == wng::Result::Ok);
        return id;
    }

    wng::PortId add_port(
        wng::Graph& graph,
        wng::NodeId node,
        wng::PortKind kind,
        const std::string& name,
        const std::string& type)
    {
        wng::PortDesc desc;
        desc.kind = kind;
        desc.name = name;
        desc.type = type;

        wng::PortId id;
        assert(graph.add_port(node, desc, &id) == wng::Result::Ok);
        return id;
    }

    // Builds the smallest graph/schema pair that should pass both built-in and
    // schema-aware connection validation. Individual tests mutate this fixture
    // to verify specific rejection paths.
    Fixture make_valid_fixture()
    {
        Fixture fixture;
        fixture.source = create_node(fixture.graph, "constant.number", "Source");
        fixture.target = create_node(fixture.graph, "debug.print", "Print");
        fixture.output = add_port(
            fixture.graph,
            fixture.source,
            wng::PortKind::Output,
            "value",
            "number");
        fixture.input = add_port(
            fixture.graph,
            fixture.target,
            wng::PortKind::Input,
            "value",
            "number");

        assert(fixture.schema.add_node_definition(source_definition()) == wng::Result::Ok);
        assert(fixture.schema.add_node_definition(target_definition()) == wng::Result::Ok);

        return fixture;
    }
}

int main()
{
    {
        // Verifies the schema-aware happy path: validation succeeds first, then
        // the helper delegates actual mutation to Graph::create_link.
        Fixture fixture = make_valid_fixture();

        wng::LinkId link;
        assert(wng::create_link(
            fixture.graph,
            fixture.schema,
            fixture.output,
            fixture.input,
            &link) == wng::Result::Ok);

        assert(link != wng::LinkId {});
        assert(fixture.graph.links().size() == 1U);
        assert(fixture.graph.links()[0].from == fixture.output);
        assert(fixture.graph.links()[0].to == fixture.input);
    }

    {
        // Null output pointers must fail before validation or mutation so
        // callers never observe a partially-created link.
        Fixture fixture = make_valid_fixture();

        assert(wng::create_link(
            fixture.graph,
            fixture.schema,
            fixture.output,
            fixture.input,
            nullptr) == wng::Result::InvalidArgument);

        assert(fixture.graph.links().empty());
    }

    {
        // Schema rejection prevents mutation and leaves caller-owned output IDs
        // unchanged even though the underlying graph connection is structural.
        Fixture fixture = make_valid_fixture();

        wng::GraphSchema schema;
        assert(schema.add_node_definition(source_definition()) == wng::Result::Ok);
        wng::NodeDefinition target = target_definition();
        target.inputs[0].name = "missing";
        assert(schema.add_node_definition(target) == wng::Result::Ok);

        wng::LinkId link { 77 };
        assert(wng::create_link(
            fixture.graph,
            schema,
            fixture.output,
            fixture.input,
            &link) == wng::Result::NotFound);

        assert(link == wng::LinkId { 77 });
        assert(fixture.graph.links().empty());
    }

    {
        // Built-in rejection remains final through the schema-aware helper. A
        // same-node connection cannot be permitted by matching schema metadata.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "constant.number", "Node");
        const wng::PortId output = add_port(graph, node, wng::PortKind::Output, "value", "number");
        const wng::PortId input = add_port(graph, node, wng::PortKind::Input, "value", "number");

        wng::GraphSchema schema;
        assert(schema.add_node_definition(source_definition()) == wng::Result::Ok);
        assert(schema.add_node_definition(target_definition()) == wng::Result::Ok);

        wng::LinkId link { 88 };
        assert(wng::create_link(graph, schema, output, input, &link) ==
            wng::Result::InvalidConnection);

        assert(link == wng::LinkId { 88 });
        assert(graph.links().empty());
    }

    {
        // Backward compatibility guard: Graph::create_link remains built-in-only
        // and still succeeds when no matching schema exists.
        Fixture fixture = make_valid_fixture();
        wng::GraphSchema empty_schema;
        (void)empty_schema;

        wng::LinkId link;
        assert(fixture.graph.create_link(fixture.output, fixture.input, &link) == wng::Result::Ok);

        assert(link != wng::LinkId {});
        assert(fixture.graph.links().size() == 1U);
    }

    {
        // Duplicate link rejection still comes from graph core. The helper must
        // not duplicate or weaken Graph's existing exact-link rule.
        Fixture fixture = make_valid_fixture();

        wng::LinkId first;
        assert(wng::create_link(
            fixture.graph,
            fixture.schema,
            fixture.output,
            fixture.input,
            &first) == wng::Result::Ok);

        wng::LinkId second { 99 };
        assert(wng::create_link(
            fixture.graph,
            fixture.schema,
            fixture.output,
            fixture.input,
            &second) == wng::Result::AlreadyExists);

        assert(second == wng::LinkId { 99 });
        assert(fixture.graph.links().size() == 1U);
    }

    {
        // Multiple links into one input remain forbidden by graph core after the
        // schema layer allows the second source output.
        Fixture fixture = make_valid_fixture();

        const wng::NodeId second_source =
            create_node(fixture.graph, "constant.number", "Second Source");
        const wng::PortId second_output = add_port(
            fixture.graph,
            second_source,
            wng::PortKind::Output,
            "value",
            "number");

        wng::LinkId first;
        assert(wng::create_link(
            fixture.graph,
            fixture.schema,
            fixture.output,
            fixture.input,
            &first) == wng::Result::Ok);

        wng::LinkId second { 100 };
        assert(wng::create_link(
            fixture.graph,
            fixture.schema,
            second_output,
            fixture.input,
            &second) == wng::Result::InvalidConnection);

        assert(second == wng::LinkId { 100 });
        assert(fixture.graph.links().size() == 1U);
    }

    return 0;
}
