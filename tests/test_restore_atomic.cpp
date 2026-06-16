// Exercises failed graph object restore atomicity for generated IDs.
// Invalid restore snapshots must not replace graph contents or advance future
// stable ID allocation.

#include <cassert>

#include <wng/graph_restore.hpp>

namespace
{
    wng::NodeDesc make_node_desc(const char* title)
    {
        wng::NodeDesc desc;
        desc.type = "test.node";
        desc.title = title;
        desc.size = wng::Vec2 { 100.0f, 50.0f };
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

    wng::GraphObjectSnapshot make_invalid_snapshot()
    {
        wng::GraphObjectSnapshot snapshot;

        wng::Node valid;
        valid.id = wng::NodeId { 100 };
        valid.title = "Restored";
        valid.size = wng::Vec2 { 100.0f, 50.0f };

        wng::Port missing_parent;
        missing_parent.id = wng::PortId { 100 };
        missing_parent.node = wng::NodeId { 999 };
        missing_parent.kind = wng::PortKind::Input;
        missing_parent.name = "missing_parent";
        missing_parent.type = "number";

        snapshot.nodes.push_back(valid);
        snapshot.ports.push_back(missing_parent);
        return snapshot;
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

    const wng::GraphRestoreResult result =
        wng::restore_graph_objects(graph, make_invalid_snapshot());
    assert(result.result == wng::Result::NotFound);
    assert(!result.success());
    assert(result.restored_nodes.empty());
    assert(result.restored_ports.empty());
    assert(result.restored_links.empty());

    assert(graph.nodes().size() == 2U);
    assert(graph.ports().size() == 2U);
    assert(graph.links().size() == 1U);
    assert(graph.find_node(source) != nullptr);
    assert(graph.find_node(target) != nullptr);
    assert(graph.find_port(output) != nullptr);
    assert(graph.find_port(input) != nullptr);
    assert(graph.find_link(link) != nullptr);
    assert(graph.find_node(wng::NodeId { 100 }) == nullptr);
    assert(graph.find_port(wng::PortId { 100 }) == nullptr);

    const wng::NodeId next_node = create_node(graph, "Next");
    assert(next_node == wng::NodeId { 3 });

    const wng::PortId next_input =
        add_port(graph, next_node, wng::PortKind::Input, "next_in");
    assert(next_input == wng::PortId { 3 });

    const wng::LinkId next_link = create_link(graph, output, next_input);
    assert(next_link == wng::LinkId { 2 });

    return 0;
}
