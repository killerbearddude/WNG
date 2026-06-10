#include <cassert>
#include <string>

#include <wng/schema.hpp>

namespace
{
    wng::PortDefinition input(const std::string& name, const std::string& type = "number")
    {
        wng::PortDefinition port;
        port.name = name;
        port.kind = wng::PortKind::Input;
        port.type = type;
        port.required = true;
        return port;
    }

    wng::PortDefinition output(const std::string& name, const std::string& type = "number")
    {
        wng::PortDefinition port;
        port.name = name;
        port.kind = wng::PortKind::Output;
        port.type = type;
        return port;
    }

    wng::NodeDefinition add_node_definition()
    {
        wng::NodeDefinition definition;
        definition.type = "math.add";
        definition.display_name = "Add";
        definition.inputs.push_back(input("a"));
        definition.inputs.push_back(input("b"));
        definition.outputs.push_back(output("result"));
        return definition;
    }
}

int main()
{
    {
        wng::GraphSchema schema;
        assert(schema.node_definitions().empty());
        assert(schema.find_node_definition("missing") == nullptr);
        assert(!schema.allows_node_type("missing"));
    }

    {
        wng::GraphSchema schema;
        const wng::NodeDefinition definition = add_node_definition();

        assert(schema.add_node_definition(definition) == wng::Result::Ok);
        assert(schema.node_definitions().size() == 1);

        const wng::NodeDefinition* found = schema.find_node_definition("math.add");
        assert(found != nullptr);
        assert(found->type == "math.add");
        assert(found->display_name == "Add");
        assert(found->inputs.size() == 2);
        assert(found->outputs.size() == 1);
        assert(found->inputs[0].name == "a");
        assert(found->inputs[1].name == "b");
        assert(found->outputs[0].name == "result");
        assert(found->inputs[0].required);
        assert(schema.allows_node_type("math.add"));
    }

    {
        wng::GraphSchema schema;
        wng::NodeDefinition definition = add_node_definition();
        definition.type.clear();

        assert(schema.add_node_definition(definition) == wng::Result::InvalidArgument);
        assert(schema.node_definitions().empty());
    }

    {
        wng::GraphSchema schema;
        const wng::NodeDefinition definition = add_node_definition();

        assert(schema.add_node_definition(definition) == wng::Result::Ok);
        assert(schema.add_node_definition(definition) == wng::Result::AlreadyExists);
        assert(schema.node_definitions().size() == 1);
    }

    {
        wng::GraphSchema schema;
        wng::NodeDefinition definition = add_node_definition();
        definition.inputs[0].kind = wng::PortKind::Output;

        assert(schema.add_node_definition(definition) == wng::Result::InvalidArgument);
        assert(schema.node_definitions().empty());
    }

    {
        wng::GraphSchema schema;
        wng::NodeDefinition definition = add_node_definition();
        definition.outputs[0].kind = wng::PortKind::Input;

        assert(schema.add_node_definition(definition) == wng::Result::InvalidArgument);
        assert(schema.node_definitions().empty());
    }

    {
        wng::GraphSchema schema;
        wng::NodeDefinition definition = add_node_definition();
        definition.inputs[1].name = definition.inputs[0].name;

        assert(schema.add_node_definition(definition) == wng::Result::AlreadyExists);
        assert(schema.node_definitions().empty());
    }

    {
        wng::GraphSchema schema;
        wng::NodeDefinition definition = add_node_definition();
        definition.outputs.push_back(output("result"));

        assert(schema.add_node_definition(definition) == wng::Result::AlreadyExists);
        assert(schema.node_definitions().empty());
    }

    {
        wng::GraphSchema schema;
        wng::NodeDefinition definition;
        definition.type = "passthrough";
        definition.inputs.push_back(input("value", "any"));
        definition.outputs.push_back(output("value", "any"));

        assert(schema.add_node_definition(definition) == wng::Result::Ok);
        assert(schema.node_definitions().size() == 1);
    }

    {
        wng::GraphSchema schema;

        wng::NodeDefinition first;
        first.type = "first";
        first.outputs.push_back(output("out"));

        wng::NodeDefinition second;
        second.type = "second";
        second.inputs.push_back(input("in"));

        assert(schema.add_node_definition(first) == wng::Result::Ok);
        assert(schema.add_node_definition(second) == wng::Result::Ok);
        assert(schema.node_definitions().size() == 2);
        assert(schema.node_definitions()[0].type == "first");
        assert(schema.node_definitions()[0].outputs[0].name == "out");
        assert(schema.node_definitions()[1].type == "second");
        assert(schema.node_definitions()[1].inputs[0].name == "in");
    }

    return 0;
}
