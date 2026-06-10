#include <cassert>
#include <string>

#include <wng/graph.hpp>
#include <wng/serialization.hpp>

namespace
{
    wng::NodeId create_node(
        wng::Graph& graph,
        const std::string& type,
        const std::string& title,
        wng::Vec2 position,
        wng::Vec2 size,
        bool visible,
        bool enabled)
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = title;
        desc.position = position;
        desc.size = size;
        desc.visible = visible;
        desc.enabled = enabled;

        wng::NodeId id;
        assert(graph.create_node(desc, &id) == wng::Result::Ok);
        return id;
    }

    wng::PortId add_port(
        wng::Graph& graph,
        wng::NodeId node,
        wng::PortKind kind,
        const std::string& name,
        const std::string& type,
        bool visible,
        bool enabled)
    {
        wng::PortDesc desc;
        desc.kind = kind;
        desc.name = name;
        desc.type = type;
        desc.visible = visible;
        desc.enabled = enabled;

        wng::PortId id;
        assert(graph.add_port(node, desc, &id) == wng::Result::Ok);
        return id;
    }
}

int main()
{
    {
        wng::Graph graph;
        wng::GraphDto dto;
        assert(wng::export_graph(graph, &dto) == wng::Result::Ok);

        assert(dto.version.major == 0);
        assert(dto.version.minor == 2);
        assert(dto.version.patch == 0);
        assert(dto.nodes.empty());
        assert(dto.ports.empty());
        assert(dto.links.empty());
    }

    {
        wng::Graph graph;

        const wng::NodeId source = create_node(
            graph,
            "constant.number",
            "Source",
            wng::Vec2 { 10.0f, 20.0f },
            wng::Vec2 { 100.0f, 50.0f },
            false,
            true);

        const wng::NodeId sink = create_node(
            graph,
            "debug.print",
            "Sink",
            wng::Vec2 { 240.0f, 20.0f },
            wng::Vec2 { 120.0f, 60.0f },
            true,
            true);

        const wng::PortId output = add_port(
            graph,
            source,
            wng::PortKind::Output,
            "value",
            "number",
            true,
            true);

        const wng::PortId input = add_port(
            graph,
            sink,
            wng::PortKind::Input,
            "value",
            "number",
            false,
            true);

        wng::LinkId link;
        assert(graph.create_link(output, input, &link) == wng::Result::Ok);

        wng::GraphDto dto;
        assert(wng::export_graph(graph, &dto) == wng::Result::Ok);

        assert(dto.nodes.size() == 2);
        assert(dto.ports.size() == 2);
        assert(dto.links.size() == 1);

        assert(dto.nodes[0].id == source);
        assert(dto.nodes[0].type == "constant.number");
        assert(dto.nodes[0].title == "Source");
        assert(dto.nodes[0].position.x == 10.0f);
        assert(dto.nodes[0].position.y == 20.0f);
        assert(dto.nodes[0].size.x == 100.0f);
        assert(dto.nodes[0].size.y == 50.0f);
        assert(dto.nodes[0].outputs.size() == 1);
        assert(dto.nodes[0].outputs[0] == output);
        assert(dto.nodes[0].visible == false);
        assert(dto.nodes[0].enabled == true);

        assert(dto.nodes[1].id == sink);
        assert(dto.nodes[1].type == "debug.print");
        assert(dto.nodes[1].title == "Sink");
        assert(dto.nodes[1].inputs.size() == 1);
        assert(dto.nodes[1].inputs[0] == input);

        assert(dto.ports[0].id == output);
        assert(dto.ports[0].node == source);
        assert(dto.ports[0].kind == wng::PortKind::Output);
        assert(dto.ports[0].name == "value");
        assert(dto.ports[0].type == "number");
        assert(dto.ports[0].visible == true);
        assert(dto.ports[0].enabled == true);

        assert(dto.ports[1].id == input);
        assert(dto.ports[1].node == sink);
        assert(dto.ports[1].kind == wng::PortKind::Input);
        assert(dto.ports[1].visible == false);
        assert(dto.ports[1].enabled == true);

        assert(dto.links[0].id == link);
        assert(dto.links[0].from == output);
        assert(dto.links[0].to == input);
        assert(dto.links[0].visible == true);
        assert(dto.links[0].enabled == true);
    }

    {
        wng::Graph graph;
        wng::GraphDto dto;
        dto.nodes.push_back(wng::NodeDto {});
        dto.ports.push_back(wng::PortDto {});
        dto.links.push_back(wng::LinkDto {});

        assert(wng::export_graph(graph, nullptr) == wng::Result::InvalidArgument);

        assert(dto.nodes.size() == 1);
        assert(dto.ports.size() == 1);
        assert(dto.links.size() == 1);
    }

    return 0;
}
