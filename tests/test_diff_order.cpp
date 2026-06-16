// Exercises deterministic GraphDiff ordering for ports and links.
// Diff entries must stay phase-ordered by removed, added, then modified.

#include <cassert>

#include <wng/graph_diff.hpp>
#include <wng/serialization.hpp>

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

    wng::GraphDto export_dto(const wng::Graph& graph)
    {
        wng::GraphDto dto;
        assert(wng::export_graph(graph, &dto) == wng::Result::Ok);
        return dto;
    }

    wng::Graph graph_from_dto(const wng::GraphDto& dto)
    {
        wng::Graph graph;
        assert(wng::import_graph(dto, &graph) == wng::Result::Ok);
        return graph;
    }
}

int main()
{
    {
        // Ports should be reported as removed-before-order, added-after-order,
        // then modified-before-order.
        wng::Graph before;
        const wng::NodeId node = create_node(before, "Node");
        const wng::PortId removed_a =
            add_port(before, node, wng::PortKind::Input, "removed_a");
        const wng::PortId modified_b =
            add_port(before, node, wng::PortKind::Input, "modified_b");
        const wng::PortId removed_c =
            add_port(before, node, wng::PortKind::Input, "removed_c");

        wng::GraphDto dto = export_dto(before);
        dto.ports.erase(dto.ports.begin() + 2);
        dto.ports.erase(dto.ports.begin());
        dto.nodes[0].inputs.clear();
        dto.nodes[0].inputs.push_back(modified_b);
        dto.ports[0].name = "modified_b_after";

        wng::PortDto added_first;
        added_first.id = wng::PortId { 100 };
        added_first.node = node;
        added_first.kind = wng::PortKind::Input;
        added_first.name = "added_first";
        added_first.type = "number";

        wng::PortDto added_second = added_first;
        added_second.id = wng::PortId { 101 };
        added_second.name = "added_second";

        dto.ports.push_back(added_first);
        dto.ports.push_back(added_second);
        dto.nodes[0].inputs.push_back(added_first.id);
        dto.nodes[0].inputs.push_back(added_second.id);
        const wng::Graph after = graph_from_dto(dto);

        const wng::GraphDiff diff = wng::diff_graphs(before, after);

        assert(diff.result == wng::Result::Ok);
        assert(diff.ports.size() == 5U);
        assert(diff.ports[0].change == wng::GraphDiffChange::Removed);
        assert(diff.ports[0].id == removed_a);
        assert(diff.ports[1].change == wng::GraphDiffChange::Removed);
        assert(diff.ports[1].id == removed_c);
        assert(diff.ports[2].change == wng::GraphDiffChange::Added);
        assert(diff.ports[2].id == added_first.id);
        assert(diff.ports[3].change == wng::GraphDiffChange::Added);
        assert(diff.ports[3].id == added_second.id);
        assert(diff.ports[4].change == wng::GraphDiffChange::Modified);
        assert(diff.ports[4].id == modified_b);
    }

    {
        // Links should follow the same deterministic ordering contract.
        wng::Graph before;
        const wng::NodeId source = create_node(before, "Source");
        const wng::NodeId target = create_node(before, "Target");
        const wng::PortId out_1 = add_port(before, source, wng::PortKind::Output, "out_1");
        const wng::PortId out_2 = add_port(before, source, wng::PortKind::Output, "out_2");
        const wng::PortId out_3 = add_port(before, source, wng::PortKind::Output, "out_3");
        const wng::PortId out_4 = add_port(before, source, wng::PortKind::Output, "out_4");
        const wng::PortId out_5 = add_port(before, source, wng::PortKind::Output, "out_5");
        const wng::PortId in_1 = add_port(before, target, wng::PortKind::Input, "in_1");
        const wng::PortId in_2 = add_port(before, target, wng::PortKind::Input, "in_2");
        const wng::PortId in_3 = add_port(before, target, wng::PortKind::Input, "in_3");
        const wng::PortId in_4 = add_port(before, target, wng::PortKind::Input, "in_4");
        const wng::PortId in_5 = add_port(before, target, wng::PortKind::Input, "in_5");
        const wng::LinkId removed_a = create_link(before, out_1, in_1);
        const wng::LinkId modified_b = create_link(before, out_2, in_2);
        const wng::LinkId removed_c = create_link(before, out_3, in_3);

        wng::GraphDto dto = export_dto(before);
        dto.links.erase(dto.links.begin() + 2);
        dto.links.erase(dto.links.begin());
        dto.links[0].visible = false;

        wng::LinkDto added_first;
        added_first.id = wng::LinkId { 100 };
        added_first.from = out_4;
        added_first.to = in_4;

        wng::LinkDto added_second;
        added_second.id = wng::LinkId { 101 };
        added_second.from = out_5;
        added_second.to = in_5;

        dto.links.push_back(added_first);
        dto.links.push_back(added_second);
        const wng::Graph after = graph_from_dto(dto);

        const wng::GraphDiff diff = wng::diff_graphs(before, after);

        assert(diff.result == wng::Result::Ok);
        assert(diff.links.size() == 5U);
        assert(diff.links[0].change == wng::GraphDiffChange::Removed);
        assert(diff.links[0].id == removed_a);
        assert(diff.links[1].change == wng::GraphDiffChange::Removed);
        assert(diff.links[1].id == removed_c);
        assert(diff.links[2].change == wng::GraphDiffChange::Added);
        assert(diff.links[2].id == added_first.id);
        assert(diff.links[3].change == wng::GraphDiffChange::Added);
        assert(diff.links[3].id == added_second.id);
        assert(diff.links[4].change == wng::GraphDiffChange::Modified);
        assert(diff.links[4].id == modified_b);
    }

    return 0;
}
