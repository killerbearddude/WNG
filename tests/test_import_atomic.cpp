// Exercises failed import atomicity for graph state and generated IDs.
// Invalid DTO imports must not replace existing graph contents or advance future
// stable ID allocation.

#include <cassert>

#include <wng/serialization.hpp>

namespace
{
    wng::NodeDesc make_node_desc(const char* title)
    {
        wng::NodeDesc desc;
        desc.type = "test.node";
        desc.title = title;
        return desc;
    }

    wng::PortDesc make_port_desc(wng::PortKind kind, const char* name)
    {
        wng::PortDesc desc;
        desc.kind = kind;
        desc.name = name;
        desc.type = "number";
        return desc;
    }

    wng::NodeId create_node(wng::Graph& graph, const char* title)
    {
        wng::NodeId id;
        assert(graph.create_node(make_node_desc(title), &id) == wng::Result::Ok);
        return id;
    }

    wng::PortId add_port(
        wng::Graph& graph,
        wng::NodeId node,
        wng::PortKind kind,
        const char* name)
    {
        wng::PortId id;
        assert(graph.add_port(node, make_port_desc(kind, name), &id) ==
            wng::Result::Ok);
        return id;
    }

    wng::LinkId create_link(wng::Graph& graph, wng::PortId from, wng::PortId to)
    {
        wng::LinkId id;
        assert(graph.create_link(from, to, &id) == wng::Result::Ok);
        return id;
    }

    wng::GraphDto make_duplicate_node_dto()
    {
        wng::GraphDto dto;

        wng::NodeDto first;
        first.id = wng::NodeId { 1 };
        first.title = "First";
        first.size = wng::Vec2 { 100.0f, 50.0f };

        wng::NodeDto second = first;
        second.title = "Duplicate";

        dto.nodes.push_back(first);
        dto.nodes.push_back(second);
        return dto;
    }
}

int main()
{
    wng::Graph graph;
    const wng::NodeId source = create_node(graph, "Source");
    const wng::NodeId target = create_node(graph, "Target");
    const wng::PortId output = add_port(graph, source, wng::PortKind::Output, "out");
    const wng::PortId input = add_port(graph, target, wng::PortKind::Input, "in");
    const wng::LinkId link = create_link(graph, output, input);

    assert(wng::import_graph(make_duplicate_node_dto(), &graph) ==
        wng::Result::AlreadyExists);

    assert(graph.nodes().size() == 2U);
    assert(graph.ports().size() == 2U);
    assert(graph.links().size() == 1U);
    assert(graph.find_node(source) != nullptr);
    assert(graph.find_node(target) != nullptr);
    assert(graph.find_port(output) != nullptr);
    assert(graph.find_port(input) != nullptr);
    assert(graph.find_link(link) != nullptr);

    const wng::NodeId next_node = create_node(graph, "Next");
    assert(next_node == wng::NodeId { 3 });

    const wng::PortId next_input =
        add_port(graph, next_node, wng::PortKind::Input, "next_in");
    assert(next_input == wng::PortId { 3 });

    const wng::LinkId next_link = create_link(graph, output, next_input);
    assert(next_link == wng::LinkId { 2 });

    return 0;
}
