// Exercises schema-aware connection validation as a pure validation layer.
// The tests intentionally avoid changing Graph mutation behavior so future
// schema-aware mutations can build on this API without weakening core rules.

#include <cassert>
#include <string>

#include <wng/schema_validation.hpp>

namespace
{
    wng::PortDefinition make_output_definition(
        const std::string& name = "value",
        const std::string& type = "number")
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Output;
        definition.type = type;
        return definition;
    }

    wng::PortDefinition make_input_definition(
        const std::string& name = "value",
        const std::string& type = "number")
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Input;
        definition.type = type;
        return definition;
    }

    wng::NodeDefinition make_source_definition()
    {
        wng::NodeDefinition definition;
        definition.type = "constant.number";
        definition.display_name = "Number Constant";
        definition.outputs.push_back(make_output_definition());
        return definition;
    }

    wng::NodeDefinition make_target_definition()
    {
        wng::NodeDefinition definition;
        definition.type = "debug.print";
        definition.display_name = "Debug Print";
        definition.inputs.push_back(make_input_definition());
        return definition;
    }

    void add_default_schema(wng::GraphSchema& schema)
    {
        assert(schema.add_node_definition(make_source_definition()) == wng::Result::Ok);
        assert(schema.add_node_definition(make_target_definition()) == wng::Result::Ok);
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

    struct GraphFixture {
        wng::Graph graph;
        wng::NodeId source;
        wng::NodeId target;
        wng::PortId output;
        wng::PortId input;
    };

    GraphFixture make_valid_graph()
    {
        GraphFixture fixture;
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
        return fixture;
    }

    void assert_validation(
        const wng::ConnectionValidation& validation,
        wng::ConnectionStatus expected_status,
        wng::Result expected_result)
    {
        assert(validation.status == expected_status);
        assert(validation.result == expected_result);
    }
}

int main()
{
    {
        // Verifies that schema validation cannot override core graph rejection.
        // This protects the rule that built-in structural validation is final.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "constant.number", "Node");
        const wng::PortId output = add_port(graph, node, wng::PortKind::Output, "out", "number");
        const wng::PortId input = add_port(graph, node, wng::PortKind::Input, "in", "number");

        wng::GraphSchema schema;
        add_default_schema(schema);

        const wng::ConnectionValidation validation =
            wng::validate_connection(graph, schema, output, input);
        assert_validation(validation, wng::ConnectionStatus::Rejected, wng::Result::InvalidConnection);
    }

    {
        // Verifies the happy path for a graph whose node and port metadata
        // exactly match registered schema definitions.
        GraphFixture fixture = make_valid_graph();

        wng::GraphSchema schema;
        add_default_schema(schema);

        const wng::ConnectionValidation validation =
            wng::validate_connection(fixture.graph, schema, fixture.output, fixture.input);
        assert_validation(validation, wng::ConnectionStatus::Allowed, wng::Result::Ok);
    }

    {
        // Missing source node definition must reject even when the graph-level
        // structural connection is otherwise valid.
        GraphFixture fixture = make_valid_graph();
        fixture.graph.find_node(fixture.source)->type = "missing.source";

        wng::GraphSchema schema;
        add_default_schema(schema);

        const wng::ConnectionValidation validation =
            wng::validate_connection(fixture.graph, schema, fixture.output, fixture.input);
        assert_validation(validation, wng::ConnectionStatus::Rejected, wng::Result::NotFound);
    }

    {
        // Missing target node definition is reported as a schema lookup failure,
        // not as a graph structural failure.
        GraphFixture fixture = make_valid_graph();
        fixture.graph.find_node(fixture.target)->type = "missing.target";

        wng::GraphSchema schema;
        add_default_schema(schema);

        const wng::ConnectionValidation validation =
            wng::validate_connection(fixture.graph, schema, fixture.output, fixture.input);
        assert_validation(validation, wng::ConnectionStatus::Rejected, wng::Result::NotFound);
    }

    {
        // The source node definition exists, but it must explicitly define the
        // graph output port by kind and name before the schema allows a link.
        GraphFixture fixture = make_valid_graph();

        wng::GraphSchema schema;
        wng::NodeDefinition source = make_source_definition();
        source.outputs[0].name = "other";
        assert(schema.add_node_definition(source) == wng::Result::Ok);
        assert(schema.add_node_definition(make_target_definition()) == wng::Result::Ok);

        const wng::ConnectionValidation validation =
            wng::validate_connection(fixture.graph, schema, fixture.output, fixture.input);
        assert_validation(validation, wng::ConnectionStatus::Rejected, wng::Result::NotFound);
    }

    {
        // The target node definition exists, but it must explicitly define the
        // graph input port by kind and name before the schema allows a link.
        GraphFixture fixture = make_valid_graph();

        wng::GraphSchema schema;
        assert(schema.add_node_definition(make_source_definition()) == wng::Result::Ok);
        wng::NodeDefinition target = make_target_definition();
        target.inputs[0].name = "other";
        assert(schema.add_node_definition(target) == wng::Result::Ok);

        const wng::ConnectionValidation validation =
            wng::validate_connection(fixture.graph, schema, fixture.output, fixture.input);
        assert_validation(validation, wng::ConnectionStatus::Rejected, wng::Result::NotFound);
    }

    {
        // Disabled schema node definitions are a further restriction layered
        // on top of a structurally valid graph connection.
        GraphFixture fixture = make_valid_graph();

        wng::GraphSchema schema;
        wng::NodeDefinition source = make_source_definition();
        source.enabled = false;
        assert(schema.add_node_definition(source) == wng::Result::Ok);
        assert(schema.add_node_definition(make_target_definition()) == wng::Result::Ok);

        const wng::ConnectionValidation validation =
            wng::validate_connection(fixture.graph, schema, fixture.output, fixture.input);
        assert_validation(validation, wng::ConnectionStatus::Rejected, wng::Result::InvalidConnection);
    }

    {
        // Disabled target schema nodes reject for the same reason as disabled
        // source schema nodes: schemas may only further restrict valid links.
        GraphFixture fixture = make_valid_graph();

        wng::GraphSchema schema;
        assert(schema.add_node_definition(make_source_definition()) == wng::Result::Ok);
        wng::NodeDefinition target = make_target_definition();
        target.enabled = false;
        assert(schema.add_node_definition(target) == wng::Result::Ok);

        const wng::ConnectionValidation validation =
            wng::validate_connection(fixture.graph, schema, fixture.output, fixture.input);
        assert_validation(validation, wng::ConnectionStatus::Rejected, wng::Result::InvalidConnection);
    }

    {
        // Disabled source port definitions reject without mutating the graph or
        // requiring Graph::create_link to become schema-aware.
        GraphFixture fixture = make_valid_graph();

        wng::GraphSchema schema;
        wng::NodeDefinition source = make_source_definition();
        source.outputs[0].enabled = false;
        assert(schema.add_node_definition(source) == wng::Result::Ok);
        assert(schema.add_node_definition(make_target_definition()) == wng::Result::Ok);

        const wng::ConnectionValidation validation =
            wng::validate_connection(fixture.graph, schema, fixture.output, fixture.input);
        assert_validation(validation, wng::ConnectionStatus::Rejected, wng::Result::InvalidConnection);
    }

    {
        // Disabled target port definitions reject independently from graph port
        // enabled state, which remains owned by built-in validation.
        GraphFixture fixture = make_valid_graph();

        wng::GraphSchema schema;
        assert(schema.add_node_definition(make_source_definition()) == wng::Result::Ok);
        wng::NodeDefinition target = make_target_definition();
        target.inputs[0].enabled = false;
        assert(schema.add_node_definition(target) == wng::Result::Ok);

        const wng::ConnectionValidation validation =
            wng::validate_connection(fixture.graph, schema, fixture.output, fixture.input);
        assert_validation(validation, wng::ConnectionStatus::Rejected, wng::Result::InvalidConnection);
    }

    {
        // Graph port metadata must remain compatible with the schema port
        // definition; a mismatch means the graph no longer conforms to schema.
        GraphFixture fixture = make_valid_graph();

        wng::GraphSchema schema;
        wng::NodeDefinition source = make_source_definition();
        source.outputs[0].type = "string";
        assert(schema.add_node_definition(source) == wng::Result::Ok);
        assert(schema.add_node_definition(make_target_definition()) == wng::Result::Ok);

        const wng::ConnectionValidation validation =
            wng::validate_connection(fixture.graph, schema, fixture.output, fixture.input);
        assert_validation(validation, wng::ConnectionStatus::Rejected, wng::Result::InvalidConnection);
    }

    {
        // The schema layer preserves core's permissive early-prototyping type
        // semantics: either side may use the wildcard type name "any".
        GraphFixture fixture = make_valid_graph();

        wng::GraphSchema schema;
        wng::NodeDefinition source = make_source_definition();
        source.outputs[0].type = "any";
        assert(schema.add_node_definition(source) == wng::Result::Ok);
        assert(schema.add_node_definition(make_target_definition()) == wng::Result::Ok);

        const wng::ConnectionValidation validation =
            wng::validate_connection(fixture.graph, schema, fixture.output, fixture.input);
        assert_validation(validation, wng::ConnectionStatus::Allowed, wng::Result::Ok);
    }

    {
        // Visibility is rendering/editor-facing state. It must not affect
        // schema-aware validation while the definitions remain enabled.
        GraphFixture fixture = make_valid_graph();

        wng::GraphSchema schema;
        wng::NodeDefinition source = make_source_definition();
        source.visible = false;
        source.outputs[0].visible = false;

        wng::NodeDefinition target = make_target_definition();
        target.visible = false;
        target.inputs[0].visible = false;

        assert(schema.add_node_definition(source) == wng::Result::Ok);
        assert(schema.add_node_definition(target) == wng::Result::Ok);

        const wng::ConnectionValidation validation =
            wng::validate_connection(fixture.graph, schema, fixture.output, fixture.input);
        assert_validation(validation, wng::ConnectionStatus::Allowed, wng::Result::Ok);
    }

    return 0;
}
