// Exercises deterministic graph diffing for WNG graph snapshots.
// These tests focus on stable-ID matching and regression use cases for
// import/export, restore, undo, redo, and command behavior.

#include <cassert>
#include <vector>

#include <wng/graph_command.hpp>
#include <wng/graph_diff.hpp>
#include <wng/graph_redo.hpp>
#include <wng/graph_undo.hpp>
#include <wng/graph_validation.hpp>
#include <wng/serialization.hpp>
#include <wng/serialization_dto.hpp>

namespace
{
    wng::NodeDesc make_node_desc(const char* title = "Node")
    {
        wng::NodeDesc desc;
        desc.type = "test.node";
        desc.title = title;
        desc.position = wng::Vec2 { 1.0f, 2.0f };
        desc.size = wng::Vec2 { 100.0f, 50.0f };
        return desc;
    }

    wng::NodeId create_node(wng::Graph& graph, const char* title = "Node")
    {
        wng::NodeId node;
        assert(graph.create_node(make_node_desc(title), &node) == wng::Result::Ok);
        return node;
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

    wng::PortId add_port(
        wng::Graph& graph,
        wng::NodeId node,
        wng::PortKind kind,
        const char* name)
    {
        wng::PortId port;
        assert(graph.add_port(node, make_port_desc(kind, name), &port) == wng::Result::Ok);
        return port;
    }

    wng::LinkId create_link(wng::Graph& graph, wng::PortId from, wng::PortId to)
    {
        wng::LinkId link;
        assert(graph.create_link(from, to, &link) == wng::Result::Ok);
        return link;
    }

    wng::Graph copy_graph(const wng::Graph& graph)
    {
        wng::GraphDto dto;
        assert(wng::export_graph(graph, &dto) == wng::Result::Ok);

        wng::Graph copy;
        assert(wng::import_graph(dto, &copy) == wng::Result::Ok);
        return copy;
    }

    wng::Graph graph_from_dto(const wng::GraphDto& dto)
    {
        wng::Graph graph;
        assert(wng::import_graph(dto, &graph) == wng::Result::Ok);
        return graph;
    }

    wng::GraphDto export_dto(const wng::Graph& graph)
    {
        wng::GraphDto dto;
        assert(wng::export_graph(graph, &dto) == wng::Result::Ok);
        return dto;
    }

    void assert_empty_diff(const wng::GraphDiff& diff)
    {
        assert(diff.result == wng::Result::Ok);
        assert(diff.success());
        assert(diff.empty());
        assert(!diff.changed());
        assert(diff.nodes.empty());
        assert(diff.ports.empty());
        assert(diff.links.empty());
    }

    struct ChainGraph {
        wng::Graph graph;
        wng::NodeId a;
        wng::NodeId b;
        wng::PortId a_output;
        wng::PortId b_input;
        wng::LinkId link;
    };

    ChainGraph make_linked_pair()
    {
        ChainGraph pair;
        pair.a = create_node(pair.graph, "A");
        pair.b = create_node(pair.graph, "B");
        pair.a_output = add_port(pair.graph, pair.a, wng::PortKind::Output, "out");
        pair.b_input = add_port(pair.graph, pair.b, wng::PortKind::Input, "in");
        pair.link = create_link(pair.graph, pair.a_output, pair.b_input);
        return pair;
    }

    bool graph_valid(const wng::Graph& graph)
    {
        return wng::validate_graph(graph).valid();
    }
}

int main()
{
    {
        // Empty graphs should compare as equal. This protects the no-op baseline
        // for diagnostics that run diff before any user graph content exists.
        const wng::Graph before;
        const wng::Graph after;

        assert_empty_diff(wng::diff_graphs(before, after));
    }

    {
        // DTO round-trip copies should be byte-for-byte equivalent at the graph
        // model level. Import/export tests can use this to detect lost fields.
        ChainGraph original = make_linked_pair();
        const wng::Graph copy = copy_graph(original.graph);

        assert_empty_diff(wng::diff_graphs(original.graph, copy));
    }

    {
        // Added nodes are matched by stable NodeId and reported in after-graph
        // storage order rather than by title or position.
        wng::Graph before;
        wng::Graph after;
        const wng::NodeId node = create_node(after, "Added");

        const wng::GraphDiff diff = wng::diff_graphs(before, after);

        assert(diff.result == wng::Result::Ok);
        assert(diff.nodes.size() == 1U);
        assert(diff.nodes[0].change == wng::GraphDiffChange::Added);
        assert(diff.nodes[0].id == node);
        assert(diff.nodes[0].after.id == node);
        assert(diff.ports.empty());
        assert(diff.links.empty());
    }

    {
        // Removed nodes are reported from before-graph storage order. Future undo
        // verification depends on removed identities remaining visible in diffs.
        wng::Graph before;
        const wng::NodeId node = create_node(before, "Removed");
        wng::Graph after = copy_graph(before);

        wng::GraphMutationSummary summary;
        assert(after.destroy_node(node, &summary) == wng::Result::Ok);

        const wng::GraphDiff diff = wng::diff_graphs(before, after);

        assert(diff.nodes.size() == 1U);
        assert(diff.nodes[0].change == wng::GraphDiffChange::Removed);
        assert(diff.nodes[0].id == node);
        assert(diff.nodes[0].before.id == node);
    }

    {
        // Modified nodes retain the same identity and carry both snapshots. DTO
        // import is used here because normal graph mutation APIs do not expose
        // node field editing yet.
        wng::Graph before;
        const wng::NodeId node = create_node(before, "Before");

        wng::GraphDto dto = export_dto(before);
        dto.nodes[0].title = "After";
        dto.nodes[0].position.x = 42.0f;
        const wng::Graph after = graph_from_dto(dto);

        const wng::GraphDiff diff = wng::diff_graphs(before, after);

        assert(diff.nodes.size() == 1U);
        assert(diff.nodes[0].change == wng::GraphDiffChange::Modified);
        assert(diff.nodes[0].id == node);
        assert(diff.nodes[0].before.title == "Before");
        assert(diff.nodes[0].after.title == "After");
        assert(diff.nodes[0].after.position.x == 42.0f);
    }

    {
        // Added ports are matched by PortId. The parent node is also reported as
        // modified because its input/output vector now references the new port.
        wng::Graph before;
        const wng::NodeId node = create_node(before, "Node");
        wng::Graph after = copy_graph(before);
        const wng::PortId port = add_port(after, node, wng::PortKind::Input, "in");

        const wng::GraphDiff diff = wng::diff_graphs(before, after);

        assert(diff.ports.size() == 1U);
        assert(diff.ports[0].change == wng::GraphDiffChange::Added);
        assert(diff.ports[0].id == port);
        assert(diff.ports[0].after.id == port);
        assert(diff.nodes.size() == 1U);
        assert(diff.nodes[0].change == wng::GraphDiffChange::Modified);
    }

    {
        // Removed ports are reported separately from the parent node modification
        // caused by the node input/output vector losing the port id.
        wng::Graph before;
        const wng::NodeId node = create_node(before, "Node");
        const wng::PortId port = add_port(before, node, wng::PortKind::Input, "in");
        wng::Graph after = copy_graph(before);

        wng::GraphMutationSummary summary;
        assert(after.remove_port(port, &summary) == wng::Result::Ok);

        const wng::GraphDiff diff = wng::diff_graphs(before, after);

        assert(diff.ports.size() == 1U);
        assert(diff.ports[0].change == wng::GraphDiffChange::Removed);
        assert(diff.ports[0].before.id == port);
    }

    {
        // Modified ports carry before/after value snapshots. This protects field
        // comparisons beyond ownership and direction.
        wng::Graph before;
        const wng::NodeId node = create_node(before, "Node");
        const wng::PortId port = add_port(before, node, wng::PortKind::Input, "old");

        wng::GraphDto dto = export_dto(before);
        dto.ports[0].name = "new";
        dto.ports[0].type = "string";
        dto.ports[0].enabled = false;
        const wng::Graph after = graph_from_dto(dto);

        const wng::GraphDiff diff = wng::diff_graphs(before, after);

        assert(diff.ports.size() == 1U);
        assert(diff.ports[0].change == wng::GraphDiffChange::Modified);
        assert(diff.ports[0].id == port);
        assert(diff.ports[0].before.name == "old");
        assert(diff.ports[0].after.name == "new");
        assert(diff.ports[0].after.type == "string");
    }

    {
        // Added links are reported by LinkId. Endpoint identity and link fields
        // are stored in the after snapshot for diagnostics.
        wng::Graph before;
        const wng::NodeId a = create_node(before, "A");
        const wng::NodeId b = create_node(before, "B");
        const wng::PortId out = add_port(before, a, wng::PortKind::Output, "out");
        const wng::PortId in = add_port(before, b, wng::PortKind::Input, "in");
        wng::Graph after = copy_graph(before);
        const wng::LinkId link = create_link(after, out, in);

        const wng::GraphDiff diff = wng::diff_graphs(before, after);

        assert(diff.links.size() == 1U);
        assert(diff.links[0].change == wng::GraphDiffChange::Added);
        assert(diff.links[0].id == link);
        assert(diff.links[0].after.from == out);
        assert(diff.links[0].after.to == in);
    }

    {
        // Removed links are reported without implying node or port removal. This
        // is useful for command regression tests around link-only edits.
        ChainGraph before = make_linked_pair();
        wng::Graph after = copy_graph(before.graph);

        wng::GraphMutationSummary summary;
        assert(after.destroy_link(before.link, &summary) == wng::Result::Ok);

        const wng::GraphDiff diff = wng::diff_graphs(before.graph, after);

        assert(diff.links.size() == 1U);
        assert(diff.links[0].change == wng::GraphDiffChange::Removed);
        assert(diff.links[0].before.id == before.link);
        assert(diff.nodes.empty());
        assert(diff.ports.empty());
    }

    {
        // Modified links compare stored link fields. Endpoint changes are not
        // needed to prove link modification and would risk creating invalid graphs.
        ChainGraph before = make_linked_pair();
        wng::GraphDto dto = export_dto(before.graph);
        dto.links[0].visible = false;
        dto.links[0].enabled = false;
        const wng::Graph after = graph_from_dto(dto);

        const wng::GraphDiff diff = wng::diff_graphs(before.graph, after);

        assert(diff.links.size() == 1U);
        assert(diff.links[0].change == wng::GraphDiffChange::Modified);
        assert(diff.links[0].id == before.link);
        assert(diff.links[0].before.visible);
        assert(!diff.links[0].after.visible);
        assert(!diff.links[0].after.enabled);
    }

    {
        // Deterministic ordering groups removed, added, then modified nodes. This
        // avoids test flakes and makes future editor diagnostics reproducible.
        wng::Graph before;
        const wng::NodeId removed_a = create_node(before, "Removed A");
        const wng::NodeId modified_b = create_node(before, "Modified B");
        const wng::NodeId removed_c = create_node(before, "Removed C");

        wng::GraphDto dto = export_dto(before);
        dto.nodes.erase(dto.nodes.begin() + 2);
        dto.nodes.erase(dto.nodes.begin());
        dto.nodes[0].title = "Modified B After";

        wng::NodeDto added_first;
        added_first.id = wng::NodeId { 100 };
        added_first.type = "test.node";
        added_first.title = "Added First";
        added_first.size = wng::Vec2 { 10.0f, 10.0f };

        wng::NodeDto added_second = added_first;
        added_second.id = wng::NodeId { 101 };
        added_second.title = "Added Second";

        dto.nodes.push_back(added_first);
        dto.nodes.push_back(added_second);
        const wng::Graph after = graph_from_dto(dto);

        const wng::GraphDiff diff = wng::diff_graphs(before, after);

        assert(diff.nodes.size() == 5U);
        assert(diff.nodes[0].change == wng::GraphDiffChange::Removed);
        assert(diff.nodes[0].id == removed_a);
        assert(diff.nodes[1].change == wng::GraphDiffChange::Removed);
        assert(diff.nodes[1].id == removed_c);
        assert(diff.nodes[2].change == wng::GraphDiffChange::Added);
        assert(diff.nodes[2].id == wng::NodeId { 100 });
        assert(diff.nodes[3].change == wng::GraphDiffChange::Added);
        assert(diff.nodes[3].id == wng::NodeId { 101 });
        assert(diff.nodes[4].change == wng::GraphDiffChange::Modified);
        assert(diff.nodes[4].id == modified_b);
    }

    {
        // Undo should restore graph structure to the baseline at the model level.
        // Future history changes should preserve stable IDs and stored fields.
        wng::Graph graph;
        const wng::Graph baseline = copy_graph(graph);

        const wng::GraphCommandResult create = wng::command_create_node(graph, make_node_desc("Undo"));
        assert(create.result == wng::Result::Ok);
        assert(wng::undo_command(graph, create.record).result == wng::Result::Ok);

        assert_empty_diff(wng::diff_graphs(baseline, graph));
    }

    {
        // Redo should restore the post-command graph exactly after an undo. This
        // protects the stable-ID replay behavior used by command history.
        wng::Graph graph;
        const wng::GraphCommandResult create = wng::command_create_node(graph, make_node_desc("Redo"));
        assert(create.result == wng::Result::Ok);
        const wng::Graph post_command = copy_graph(graph);

        assert(wng::undo_command(graph, create.record).result == wng::Result::Ok);
        assert(wng::redo_command(graph, create.record).result == wng::Result::Ok);

        assert_empty_diff(wng::diff_graphs(post_command, graph));
    }

    {
        // Diffing is read-only. Both graph snapshots must retain their object
        // counts, IDs, and structural validity after comparison.
        ChainGraph before = make_linked_pair();
        wng::Graph after = copy_graph(before.graph);
        const std::vector<wng::Node> before_nodes = before.graph.nodes();
        const std::vector<wng::Port> before_ports = before.graph.ports();
        const std::vector<wng::Link> before_links = before.graph.links();
        const std::vector<wng::Node> after_nodes = after.nodes();
        const std::vector<wng::Port> after_ports = after.ports();
        const std::vector<wng::Link> after_links = after.links();

        assert_empty_diff(wng::diff_graphs(before.graph, after));

        assert(before.graph.nodes().size() == before_nodes.size());
        assert(before.graph.ports().size() == before_ports.size());
        assert(before.graph.links().size() == before_links.size());
        assert(after.nodes().size() == after_nodes.size());
        assert(after.ports().size() == after_ports.size());
        assert(after.links().size() == after_links.size());
        assert(before.graph.nodes()[0].id == before_nodes[0].id);
        assert(after.nodes()[0].id == after_nodes[0].id);
        assert(graph_valid(before.graph));
        assert(graph_valid(after));
    }

    return 0;
}
