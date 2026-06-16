// Exercises graph snapshot restore with non-contiguous high stable IDs.
// Restoring DTO-backed graph state must advance future generated IDs beyond the
// restored maxima instead of reusing persistent IDs loaded from the snapshot.

#include <cassert>
#include <vector>

#include <wng/graph_snapshot.hpp>

namespace
{
    bool contains_node_id(const std::vector<wng::NodeId>& ids, wng::NodeId id)
    {
        for (const wng::NodeId existing : ids) {
            if (existing == id) {
                return true;
            }
        }

        return false;
    }

    bool contains_port_id(const std::vector<wng::PortId>& ids, wng::PortId id)
    {
        for (const wng::PortId existing : ids) {
            if (existing == id) {
                return true;
            }
        }

        return false;
    }

    bool contains_link_id(const std::vector<wng::LinkId>& ids, wng::LinkId id)
    {
        for (const wng::LinkId existing : ids) {
            if (existing == id) {
                return true;
            }
        }

        return false;
    }

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

    wng::GraphSnapshot make_high_id_snapshot()
    {
        wng::GraphSnapshot snapshot;

        wng::NodeDto source;
        source.id = wng::NodeId { 42 };
        source.type = "test.node";
        source.title = "Source";
        source.outputs.push_back(wng::PortId { 77 });
        snapshot.graph.nodes.push_back(source);

        wng::NodeDto target;
        target.id = wng::NodeId { 84 };
        target.type = "test.node";
        target.title = "Target";
        target.inputs.push_back(wng::PortId { 88 });
        snapshot.graph.nodes.push_back(target);

        wng::PortDto output;
        output.id = wng::PortId { 77 };
        output.node = source.id;
        output.kind = wng::PortKind::Output;
        output.name = "out";
        output.type = "number";
        snapshot.graph.ports.push_back(output);

        wng::PortDto input;
        input.id = wng::PortId { 88 };
        input.node = target.id;
        input.kind = wng::PortKind::Input;
        input.name = "in";
        input.type = "number";
        snapshot.graph.ports.push_back(input);

        wng::LinkDto link;
        link.id = wng::LinkId { 99 };
        link.from = output.id;
        link.to = input.id;
        snapshot.graph.links.push_back(link);

        return snapshot;
    }
}

int main()
{
    // Imported snapshots can contain sparse persistent IDs. Future graph mutation
    // must allocate new IDs beyond those restored values instead of colliding with
    // the persistent identity model.
    wng::Graph graph;
    const wng::GraphSnapshot snapshot = make_high_id_snapshot();
    assert(wng::restore_graph_snapshot(graph, snapshot) == wng::Result::Ok);

    assert(graph.find_node(wng::NodeId { 42 }) != nullptr);
    assert(graph.find_node(wng::NodeId { 84 }) != nullptr);
    assert(graph.find_port(wng::PortId { 77 }) != nullptr);
    assert(graph.find_port(wng::PortId { 88 }) != nullptr);
    assert(graph.find_link(wng::LinkId { 99 }) != nullptr);

    wng::NodeId new_source;
    assert(graph.create_node(make_node_desc("New source"), &new_source) ==
        wng::Result::Ok);
    wng::NodeId new_target;
    assert(graph.create_node(make_node_desc("New target"), &new_target) ==
        wng::Result::Ok);

    wng::PortId new_output;
    assert(graph.add_port(
        new_source,
        make_port_desc(wng::PortKind::Output, "new_out"),
        &new_output) == wng::Result::Ok);
    wng::PortId new_input;
    assert(graph.add_port(
        new_target,
        make_port_desc(wng::PortKind::Input, "new_in"),
        &new_input) == wng::Result::Ok);

    wng::LinkId new_link;
    assert(graph.create_link(new_output, new_input, &new_link) == wng::Result::Ok);

    const std::vector<wng::NodeId> restored_nodes { wng::NodeId { 42 }, wng::NodeId { 84 } };
    const std::vector<wng::PortId> restored_ports { wng::PortId { 77 }, wng::PortId { 88 } };
    const std::vector<wng::LinkId> restored_links { wng::LinkId { 99 } };

    assert(!contains_node_id(restored_nodes, new_source));
    assert(!contains_node_id(restored_nodes, new_target));
    assert(!contains_port_id(restored_ports, new_output));
    assert(!contains_port_id(restored_ports, new_input));
    assert(!contains_link_id(restored_links, new_link));

    return 0;
}
