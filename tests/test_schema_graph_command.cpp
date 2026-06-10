// Exercises schema-aware graph command helpers.
// These tests verify command records for schema-routed mutations without adding
// undo/redo history, transactions, editor state, or WPL integration.

#include <cassert>

#include <wng/graph_command.hpp>
#include <wng/schema.hpp>

namespace
{
    wng::PortDefinition input_definition(
        const char* name,
        const char* type = "number",
        bool enabled = true)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Input;
        definition.type = type;
        definition.required = true;
        definition.enabled = enabled;
        return definition;
    }

    wng::PortDefinition output_definition(
        const char* name,
        const char* type = "number",
        bool enabled = true)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Output;
        definition.type = type;
        definition.required = true;
        definition.enabled = enabled;
        return definition;
    }

    wng::NodeDefinition make_definition(
        const char* type = "math.add",
        bool enabled = true)
    {
        wng::NodeDefinition definition;
        definition.type = type;
        definition.display_name = "Add";
        definition.inputs.push_back(input_definition("a"));
        definition.outputs.push_back(output_definition("result"));
        definition.enabled = enabled;
        return definition;
    }

    wng::GraphSchema schema_with(const wng::NodeDefinition& definition)
    {
        wng::GraphSchema schema;
        assert(schema.add_node_definition(definition) == wng::Result::Ok);
        return schema;
    }

    wng::NodeDesc make_desc(const char* type = "math.add")
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = "Add";
        return desc;
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

    wng::NodeId create_schema_node(
        wng::Graph& graph,
        const wng::GraphSchema& schema,
        const char* type = "math.add")
    {
        const wng::GraphCommandResult result =
            wng::command_create_node(graph, schema, make_desc(type));
        assert(result.result == wng::Result::Ok);
        return result.record.node;
    }

    wng::PortId add_schema_port(
        wng::Graph& graph,
        const wng::GraphSchema& schema,
        wng::NodeId node,
        wng::PortKind kind,
        const char* name)
    {
        const wng::GraphCommandResult result =
            wng::command_add_port(graph, schema, node, make_port_desc(kind, name));
        assert(result.result == wng::Result::Ok);
        return result.record.port;
    }
}

int main()
{
    {
        // Schema create-node commands route through schema mutation and record the
        // same forward data as graph-only creation.
        wng::Graph graph;
        const wng::GraphSchema schema = schema_with(make_definition());
        const wng::NodeDesc desc = make_desc();

        const wng::GraphCommandResult result =
            wng::command_create_node(graph, schema, desc);

        assert(result.result == wng::Result::Ok);
        assert(result.record.kind == wng::GraphCommandKind::SchemaCreateNode);
        assert(result.record.node_desc.type == desc.type);
        assert(result.record.node_desc.title == desc.title);
        assert(result.record.node != wng::NodeId {});
        assert(result.record.created_nodes.size() == 1U);
        assert(result.record.created_nodes[0].id == result.record.node);
        assert(graph.find_node(result.record.node) != nullptr);
    }

    {
        // Unknown node types are rejected by the schema mutation layer, and the
        // command wrapper must not fabricate created-object records.
        wng::Graph graph;
        const wng::GraphSchema schema = schema_with(make_definition());

        const wng::GraphCommandResult result =
            wng::command_create_node(graph, schema, make_desc("missing.type"));

        assert(result.result == wng::Result::NotFound);
        assert(result.record.kind == wng::GraphCommandKind::SchemaCreateNode);
        assert(result.record.node == wng::NodeId {});
        assert(result.record.created_nodes.empty());
        assert(graph.nodes().empty());
    }

    {
        // Schema instantiation creates a node plus schema-declared ports and
        // records those created snapshots for future undo integration.
        wng::Graph graph;
        const wng::GraphSchema schema = schema_with(make_definition());

        const wng::GraphCommandResult result =
            wng::command_instantiate_node(graph, schema, make_desc());

        assert(result.result == wng::Result::Ok);
        assert(result.record.kind == wng::GraphCommandKind::SchemaInstantiateNode);
        assert(result.record.node != wng::NodeId {});
        assert(result.record.created_nodes.size() == 1U);
        assert(result.record.created_nodes[0].id == result.record.node);
        assert(result.record.created_ports.size() == 2U);
        assert(graph.find_node(result.record.node) != nullptr);
        assert(graph.ports().size() == 2U);
    }

    {
        // Disabled schema definitions are rejected before graph mutation, leaving
        // no created node behind and no rollback side effects to report.
        wng::Graph graph;
        const wng::GraphSchema schema = schema_with(make_definition("math.add", false));

        const wng::GraphCommandResult result =
            wng::command_instantiate_node(graph, schema, make_desc());

        assert(result.result == wng::Result::InvalidConnection);
        assert(result.record.kind == wng::GraphCommandKind::SchemaInstantiateNode);
        assert(result.record.node == wng::NodeId {});
        assert(result.record.created_nodes.empty());
        assert(result.record.created_ports.empty());
        assert(result.record.summary.removed_nodes.empty());
        assert(result.record.summary.removed_ports.empty());
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
    }

    {
        // Schema add-port commands delegate policy to schema mutation and record
        // the created port snapshot on success.
        wng::Graph graph;
        const wng::GraphSchema schema = schema_with(make_definition());
        const wng::NodeId node = create_schema_node(graph, schema);

        const wng::PortDesc desc = make_port_desc(wng::PortKind::Input, "a");
        const wng::GraphCommandResult result =
            wng::command_add_port(graph, schema, node, desc);

        assert(result.result == wng::Result::Ok);
        assert(result.record.kind == wng::GraphCommandKind::SchemaAddPort);
        assert(result.record.node == node);
        assert(result.record.port_desc.name == desc.name);
        assert(result.record.port != wng::PortId {});
        assert(result.record.created_ports.size() == 1U);
        assert(result.record.created_ports[0].id == result.record.port);
        assert(graph.find_port(result.record.port) != nullptr);
    }

    {
        // Missing port definitions are schema failures. The command record keeps
        // descriptor context but leaves created snapshots empty.
        wng::Graph graph;
        const wng::GraphSchema schema = schema_with(make_definition());
        const wng::NodeId node = create_schema_node(graph, schema);

        const wng::GraphCommandResult result =
            wng::command_add_port(
                graph,
                schema,
                node,
                make_port_desc(wng::PortKind::Input, "missing"));

        assert(result.result == wng::Result::NotFound);
        assert(result.record.kind == wng::GraphCommandKind::SchemaAddPort);
        assert(result.record.node == node);
        assert(result.record.port == wng::PortId {});
        assert(result.record.created_ports.empty());
        assert(graph.ports().empty());
    }

    {
        // Schema create-link commands use schema-aware connection policy and
        // record the created link snapshot on success.
        wng::Graph graph;
        const wng::GraphSchema schema = schema_with(make_definition());
        const wng::NodeId source = create_schema_node(graph, schema);
        const wng::NodeId target = create_schema_node(graph, schema);
        const wng::PortId output =
            add_schema_port(graph, schema, source, wng::PortKind::Output, "result");
        const wng::PortId input =
            add_schema_port(graph, schema, target, wng::PortKind::Input, "a");

        const wng::GraphCommandResult result =
            wng::command_create_link(graph, schema, output, input);

        assert(result.result == wng::Result::Ok);
        assert(result.record.kind == wng::GraphCommandKind::SchemaCreateLink);
        assert(result.record.link != wng::LinkId {});
        assert(result.record.created_links.size() == 1U);
        assert(result.record.created_links[0].id == result.record.link);
        assert(graph.find_link(result.record.link) != nullptr);
    }

    {
        // Schema-invalid connections can be structurally valid. The schema command
        // must return the schema helper's failure and leave Graph unchanged.
        wng::NodeDefinition definition = make_definition();
        definition.inputs[0].enabled = false;

        wng::Graph graph;
        const wng::GraphSchema schema = schema_with(definition);

        const wng::NodeId source = create_schema_node(graph, schema);
        const wng::NodeId target = create_schema_node(graph, schema);

        wng::PortDesc output_desc = make_port_desc(wng::PortKind::Output, "result");
        wng::PortId output;
        assert(graph.add_port(source, output_desc, &output) == wng::Result::Ok);

        wng::PortDesc input_desc = make_port_desc(wng::PortKind::Input, "a");
        wng::PortId input;
        assert(graph.add_port(target, input_desc, &input) == wng::Result::Ok);

        const wng::GraphCommandResult result =
            wng::command_create_link(graph, schema, output, input);

        assert(result.result == wng::Result::InvalidConnection);
        assert(result.record.kind == wng::GraphCommandKind::SchemaCreateLink);
        assert(result.record.link == wng::LinkId {});
        assert(result.record.created_links.empty());
        assert(graph.links().empty());
    }

    {
        // Graph-only command helpers remain schema-free. Unknown schema/domain
        // node types are valid when the underlying Graph operation allows them.
        wng::Graph graph;
        wng::NodeDesc desc = make_desc("domain.unregistered");

        const wng::GraphCommandResult result =
            wng::command_create_node(graph, desc);

        assert(result.result == wng::Result::Ok);
        assert(result.record.kind == wng::GraphCommandKind::CreateNode);
        assert(result.record.created_nodes.size() == 1U);
        assert(graph.find_node(result.record.node)->type == desc.type);
    }

    return 0;
}
