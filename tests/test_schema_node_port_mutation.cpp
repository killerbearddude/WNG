// Exercises schema-aware node and port creation without making Graph schema-aware.
// Each test protects the opt-in mutation boundary: schema helpers validate first,
// then delegate final structural mutation to Graph.

#include <cassert>
#include <string>

#include <wng/schema_mutation.hpp>

namespace
{
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

    wng::PortDefinition make_output_definition(
        const std::string& name = "result",
        const std::string& type = "number")
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Output;
        definition.type = type;
        return definition;
    }

    wng::NodeDefinition make_node_definition(const std::string& type = "constant.number")
    {
        wng::NodeDefinition definition;
        definition.type = type;
        definition.display_name = "Number Constant";
        definition.inputs.push_back(make_input_definition());
        definition.outputs.push_back(make_output_definition());
        return definition;
    }

    wng::GraphSchema make_schema()
    {
        wng::GraphSchema schema;
        assert(schema.add_node_definition(make_node_definition()) == wng::Result::Ok);
        return schema;
    }

    wng::NodeDesc make_node_desc(const std::string& type = "constant.number")
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = "My Constant";
        return desc;
    }

    wng::PortDesc make_input_desc(
        const std::string& name = "value",
        const std::string& type = "number")
    {
        wng::PortDesc desc;
        desc.kind = wng::PortKind::Input;
        desc.name = name;
        desc.type = type;
        return desc;
    }

    wng::PortDesc make_output_desc(
        const std::string& name = "result",
        const std::string& type = "number")
    {
        wng::PortDesc desc;
        desc.kind = wng::PortKind::Output;
        desc.name = name;
        desc.type = type;
        return desc;
    }

    wng::NodeId create_schema_node(wng::Graph& graph, const wng::GraphSchema& schema)
    {
        wng::NodeId node;
        assert(wng::create_node(graph, schema, make_node_desc(), &node) == wng::Result::Ok);
        return node;
    }
}

int main()
{
    {
        // Verifies schema-aware node creation accepts known enabled node types
        // while preserving caller-supplied display title rather than auto-filling it.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();

        wng::NodeId node;
        assert(wng::create_node(graph, schema, make_node_desc(), &node) == wng::Result::Ok);

        assert(node != wng::NodeId {});
        assert(graph.nodes().size() == 1U);
        assert(graph.nodes()[0].type == "constant.number");
        assert(graph.nodes()[0].title == "My Constant");
    }

    {
        // Null node output pointers fail before schema lookup or mutation so
        // callers never observe a partially-created node.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();

        assert(wng::create_node(graph, schema, make_node_desc(), nullptr) ==
            wng::Result::InvalidArgument);
        assert(graph.nodes().empty());
    }

    {
        // Empty node types are invalid for schema-aware creation because the
        // helper cannot resolve a NodeDefinition to authorize the mutation.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();

        wng::NodeDesc desc = make_node_desc();
        desc.type.clear();

        wng::NodeId node { 42 };
        assert(wng::create_node(graph, schema, desc, &node) == wng::Result::InvalidArgument);
        assert(node == wng::NodeId { 42 });
        assert(graph.nodes().empty());
    }

    {
        // Unknown node types reject without mutating Graph. This keeps schema
        // policy opt-in and failure-atomic at the helper boundary.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();

        wng::NodeId node { 43 };
        assert(wng::create_node(graph, schema, make_node_desc("unknown.node"), &node) ==
            wng::Result::NotFound);
        assert(node == wng::NodeId { 43 });
        assert(graph.nodes().empty());
    }

    {
        // Disabled node definitions are a schema policy rejection, not a Graph
        // structural failure.
        wng::Graph graph;
        wng::GraphSchema schema;
        wng::NodeDefinition definition = make_node_definition();
        definition.enabled = false;
        assert(schema.add_node_definition(definition) == wng::Result::Ok);

        wng::NodeId node { 44 };
        assert(wng::create_node(graph, schema, make_node_desc(), &node) ==
            wng::Result::InvalidConnection);
        assert(node == wng::NodeId { 44 });
        assert(graph.nodes().empty());
    }

    {
        // Backward compatibility guard: bare Graph::create_node remains schema-free
        // and can create unknown types when the descriptor is otherwise valid.
        wng::Graph graph;
        wng::NodeId node;
        assert(graph.create_node(make_node_desc("unknown.node"), &node) == wng::Result::Ok);

        assert(node != wng::NodeId {});
        assert(graph.nodes().size() == 1U);
        assert(graph.nodes()[0].type == "unknown.node");
    }

    {
        // Verifies schema-aware port creation accepts declared input/output ports
        // and lets Graph update the node's directional port lists.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();
        const wng::NodeId node = create_schema_node(graph, schema);

        wng::PortId input;
        assert(wng::add_port(graph, schema, node, make_input_desc(), &input) == wng::Result::Ok);

        wng::PortId output;
        assert(wng::add_port(graph, schema, node, make_output_desc(), &output) == wng::Result::Ok);

        assert(input != wng::PortId {});
        assert(output != wng::PortId {});
        assert(graph.ports().size() == 2U);
        assert(graph.nodes()[0].inputs.size() == 1U);
        assert(graph.nodes()[0].outputs.size() == 1U);

        const wng::Port* input_port = graph.find_port(input);
        assert(input_port != nullptr);
        assert(input_port->kind == wng::PortKind::Input);
        assert(input_port->name == "value");
        assert(input_port->type == "number");

        const wng::Port* output_port = graph.find_port(output);
        assert(output_port != nullptr);
        assert(output_port->kind == wng::PortKind::Output);
        assert(output_port->name == "result");
        assert(output_port->type == "number");
    }

    {
        // Null port output pointers fail without mutating the target node's port
        // lists or the graph's global port storage.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();
        const wng::NodeId node = create_schema_node(graph, schema);

        assert(wng::add_port(graph, schema, node, make_input_desc(), nullptr) ==
            wng::Result::InvalidArgument);
        assert(graph.ports().empty());
        assert(graph.nodes()[0].inputs.empty());
    }

    {
        // Missing target nodes reject before schema lookup so caller-owned IDs
        // remain unchanged and no orphaned port can be created.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();

        wng::PortId port { 45 };
        assert(wng::add_port(graph, schema, wng::NodeId { 999 }, make_input_desc(), &port) ==
            wng::Result::NotFound);
        assert(port == wng::PortId { 45 });
        assert(graph.ports().empty());
    }

    {
        // Nodes created through bare Graph APIs may have unknown types. Schema-aware
        // port creation rejects those nodes because no NodeDefinition can be found.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();

        wng::NodeId node;
        assert(graph.create_node(make_node_desc("unknown.node"), &node) == wng::Result::Ok);

        wng::PortId port { 46 };
        assert(wng::add_port(graph, schema, node, make_input_desc(), &port) ==
            wng::Result::NotFound);
        assert(port == wng::PortId { 46 });
        assert(graph.ports().empty());
    }

    {
        // Port names must be declared by the node definition. Missing port
        // definitions reject without calling Graph::add_port.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();
        const wng::NodeId node = create_schema_node(graph, schema);

        wng::PortId port { 47 };
        assert(wng::add_port(graph, schema, node, make_input_desc("missing"), &port) ==
            wng::Result::NotFound);
        assert(port == wng::PortId { 47 });
        assert(graph.ports().empty());
    }

    {
        // Disabled port definitions are schema policy failures and must not
        // mutate the graph.
        wng::Graph graph;
        wng::GraphSchema schema;
        wng::NodeDefinition definition = make_node_definition();
        definition.inputs[0].enabled = false;
        assert(schema.add_node_definition(definition) == wng::Result::Ok);

        const wng::NodeId node = create_schema_node(graph, schema);

        wng::PortId port { 48 };
        assert(wng::add_port(graph, schema, node, make_input_desc(), &port) ==
            wng::Result::InvalidConnection);
        assert(port == wng::PortId { 48 });
        assert(graph.ports().empty());
    }

    {
        // Requested port type must remain compatible with the schema port type.
        // A mismatch is rejected before Graph::add_port can allocate an ID.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();
        const wng::NodeId node = create_schema_node(graph, schema);

        wng::PortId port { 49 };
        assert(wng::add_port(graph, schema, node, make_input_desc("value", "string"), &port) ==
            wng::Result::InvalidConnection);
        assert(port == wng::PortId { 49 });
        assert(graph.ports().empty());
    }

    {
        // The schema layer preserves WNG's permissive wildcard typing: a schema
        // port typed as "any" accepts a concrete requested port type.
        wng::Graph graph;
        wng::GraphSchema schema;
        wng::NodeDefinition definition = make_node_definition();
        definition.inputs[0].type = "any";
        assert(schema.add_node_definition(definition) == wng::Result::Ok);

        const wng::NodeId node = create_schema_node(graph, schema);

        wng::PortId port;
        assert(wng::add_port(graph, schema, node, make_input_desc("value", "number"), &port) ==
            wng::Result::Ok);
        assert(port != wng::PortId {});
        assert(graph.ports().size() == 1U);
    }

    {
        // Schema-aware port creation is stricter than bare Graph::add_port: it
        // rejects duplicate kind/name ports for the same node.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();
        const wng::NodeId node = create_schema_node(graph, schema);

        wng::PortId first;
        assert(wng::add_port(graph, schema, node, make_input_desc(), &first) == wng::Result::Ok);

        wng::PortId second { 50 };
        assert(wng::add_port(graph, schema, node, make_input_desc(), &second) ==
            wng::Result::AlreadyExists);
        assert(second == wng::PortId { 50 });
        assert(graph.ports().size() == 1U);
    }

    {
        // Visibility is editor/rendering state. Disabled controls validation;
        // invisible but enabled node/port definitions still allow creation.
        wng::Graph graph;
        wng::GraphSchema schema;
        wng::NodeDefinition definition = make_node_definition();
        definition.visible = false;
        definition.inputs[0].visible = false;
        assert(schema.add_node_definition(definition) == wng::Result::Ok);

        wng::NodeId node;
        assert(wng::create_node(graph, schema, make_node_desc(), &node) == wng::Result::Ok);

        wng::PortId port;
        assert(wng::add_port(graph, schema, node, make_input_desc(), &port) == wng::Result::Ok);
        assert(graph.nodes().size() == 1U);
        assert(graph.ports().size() == 1U);
    }

    {
        // Backward compatibility guard: bare Graph::add_port remains permissive
        // and can add ports that are not declared in any schema.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();
        const wng::NodeId node = create_schema_node(graph, schema);

        wng::PortId port;
        assert(graph.add_port(node, make_input_desc("undeclared"), &port) == wng::Result::Ok);

        assert(port != wng::PortId {});
        assert(graph.ports().size() == 1U);
        assert(graph.ports()[0].name == "undeclared");
    }

    return 0;
}
