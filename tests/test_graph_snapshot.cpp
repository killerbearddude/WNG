// Exercises DTO-backed graph snapshot capture and restore.
// These tests protect the snapshot layer as an in-memory graph-core boundary
// without introducing file formats, editor state, selection state, or WPL policy.

#include <cassert>
#include <vector>

#include <wng/graph_command.hpp>
#include <wng/graph_diff.hpp>
#include <wng/graph_snapshot.hpp>
#include <wng/graph_undo.hpp>

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

    wng::NodeId create_node(wng::Graph& graph, const char* title = "Node")
    {
        wng::NodeId node;
        assert(graph.create_node(make_node_desc(title), &node) == wng::Result::Ok);
        return node;
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

    struct SampleGraphIds {
        wng::NodeId source;
        wng::NodeId target;
        wng::PortId output;
        wng::PortId input;
        wng::LinkId link;
    };

    SampleGraphIds build_linked_graph(wng::Graph& graph)
    {
        SampleGraphIds ids;
        ids.source = create_node(graph, "Source");
        ids.target = create_node(graph, "Target");
        ids.output = add_port(graph, ids.source, wng::PortKind::Output, "out");
        ids.input = add_port(graph, ids.target, wng::PortKind::Input, "in");
        ids.link = create_link(graph, ids.output, ids.input);
        return ids;
    }

    wng::Graph restore_to_fresh_graph(const wng::GraphSnapshot& snapshot)
    {
        wng::Graph graph;
        assert(wng::restore_graph_snapshot(graph, snapshot) == wng::Result::Ok);
        return graph;
    }

    void assert_graphs_equal(const wng::Graph& expected, const wng::Graph& actual)
    {
        const wng::GraphDiff diff = wng::diff_graphs(expected, actual);
        assert(diff.result == wng::Result::Ok);
        assert(diff.empty());
    }

    bool contains_node_id(const std::vector<wng::NodeId>& ids, wng::NodeId id)
    {
        for (wng::NodeId existing : ids) {
            if (existing == id) {
                return true;
            }
        }

        return false;
    }

    bool contains_port_id(const std::vector<wng::PortId>& ids, wng::PortId id)
    {
        for (wng::PortId existing : ids) {
            if (existing == id) {
                return true;
            }
        }

        return false;
    }

    bool contains_link_id(const std::vector<wng::LinkId>& ids, wng::LinkId id)
    {
        for (wng::LinkId existing : ids) {
            if (existing == id) {
                return true;
            }
        }

        return false;
    }

    wng::GraphSnapshot make_invalid_snapshot_with_broken_node_port_reference()
    {
        wng::GraphSnapshot snapshot;

        wng::NodeDto node;
        node.id = wng::NodeId { 1 };
        node.type = "broken.node";
        node.title = "Broken";
        node.outputs.push_back(wng::PortId { 99 });
        snapshot.graph.nodes.push_back(node);

        return snapshot;
    }
}

int main()
{
    {
        // Verifies empty graphs can still be represented as successful snapshots.
        // Future draft-state code should not need special handling for empty graphs.
        wng::Graph graph;

        const wng::GraphSnapshotResult result = wng::capture_graph_snapshot(graph);

        assert(result.result == wng::Result::Ok);
        assert(result.success());
        assert(result.snapshot.empty());
    }

    {
        // Verifies non-empty graph content is captured into the DTO-backed
        // snapshot instead of only recording version metadata.
        wng::Graph graph;
        build_linked_graph(graph);

        const wng::GraphSnapshotResult result = wng::capture_graph_snapshot(graph);

        assert(result.result == wng::Result::Ok);
        assert(!result.snapshot.empty());
    }

    {
        // Verifies restore is replacement, not merge. Restoring an empty snapshot
        // must clear all target graph objects.
        wng::Graph empty_graph;
        const wng::GraphSnapshotResult empty_snapshot =
            wng::capture_graph_snapshot(empty_graph);
        assert(empty_snapshot.result == wng::Result::Ok);

        wng::Graph target;
        build_linked_graph(target);

        assert(wng::restore_graph_snapshot(target, empty_snapshot.snapshot) ==
            wng::Result::Ok);
        assert(target.nodes().empty());
        assert(target.ports().empty());
        assert(target.links().empty());
    }

    {
        // Verifies restore replaces unrelated target contents with the captured
        // source graph. Snapshot restore is not a patch or merge operation.
        wng::Graph source;
        build_linked_graph(source);
        const wng::GraphSnapshotResult source_snapshot =
            wng::capture_graph_snapshot(source);
        assert(source_snapshot.result == wng::Result::Ok);

        wng::Graph target;
        create_node(target, "Unrelated");

        assert(wng::restore_graph_snapshot(target, source_snapshot.snapshot) ==
            wng::Result::Ok);
        assert_graphs_equal(source, target);
    }

    {
        // Verifies snapshots preserve stable object IDs across restore. Undo/redo
        // diagnostics and editor draft comparisons depend on references surviving.
        wng::Graph original;
        const SampleGraphIds ids = build_linked_graph(original);

        const wng::GraphSnapshotResult snapshot =
            wng::capture_graph_snapshot(original);
        assert(snapshot.result == wng::Result::Ok);

        wng::Graph restored = restore_to_fresh_graph(snapshot.snapshot);

        assert(restored.find_node(ids.source) != nullptr);
        assert(restored.find_node(ids.target) != nullptr);
        assert(restored.find_port(ids.output) != nullptr);
        assert(restored.find_port(ids.input) != nullptr);
        assert(restored.find_link(ids.link) != nullptr);
        assert_graphs_equal(original, restored);
    }

    {
        // Verifies restore keeps ID allocation safe. Creating new objects after a
        // restore must not collide with IDs loaded from the snapshot.
        wng::Graph original;
        const SampleGraphIds ids = build_linked_graph(original);

        const wng::GraphSnapshotResult snapshot =
            wng::capture_graph_snapshot(original);
        assert(snapshot.result == wng::Result::Ok);

        wng::Graph restored = restore_to_fresh_graph(snapshot.snapshot);

        wng::NodeId new_source;
        assert(restored.create_node(make_node_desc("New source"), &new_source) ==
            wng::Result::Ok);
        wng::NodeId new_target;
        assert(restored.create_node(make_node_desc("New target"), &new_target) ==
            wng::Result::Ok);

        wng::PortId new_output;
        assert(restored.add_port(
            new_source,
            make_port_desc(wng::PortKind::Output, "new_out"),
            &new_output) == wng::Result::Ok);
        wng::PortId new_input;
        assert(restored.add_port(
            new_target,
            make_port_desc(wng::PortKind::Input, "new_in"),
            &new_input) == wng::Result::Ok);

        wng::LinkId new_link;
        assert(restored.create_link(new_output, new_input, &new_link) ==
            wng::Result::Ok);

        const std::vector<wng::NodeId> restored_nodes { ids.source, ids.target };
        const std::vector<wng::PortId> restored_ports { ids.output, ids.input };
        const std::vector<wng::LinkId> restored_links { ids.link };

        assert(!contains_node_id(restored_nodes, new_source));
        assert(!contains_node_id(restored_nodes, new_target));
        assert(!contains_port_id(restored_ports, new_output));
        assert(!contains_port_id(restored_ports, new_input));
        assert(!contains_link_id(restored_links, new_link));
    }

    {
        // Verifies capture is a pure query. Snapshot creation must not change
        // graph contents, object order, or stable IDs.
        wng::Graph graph;
        const SampleGraphIds ids = build_linked_graph(graph);

        const std::size_t node_count = graph.nodes().size();
        const std::size_t port_count = graph.ports().size();
        const std::size_t link_count = graph.links().size();

        const wng::GraphSnapshotResult snapshot =
            wng::capture_graph_snapshot(graph);
        assert(snapshot.result == wng::Result::Ok);

        assert(graph.nodes().size() == node_count);
        assert(graph.ports().size() == port_count);
        assert(graph.links().size() == link_count);
        assert(graph.find_node(ids.source) != nullptr);
        assert(graph.find_port(ids.output) != nullptr);
        assert(graph.find_link(ids.link) != nullptr);
    }

    {
        // Verifies failed restore is atomic. A malformed snapshot must not
        // partially overwrite a valid target graph.
        wng::Graph target;
        build_linked_graph(target);
        const wng::GraphSnapshotResult before =
            wng::capture_graph_snapshot(target);
        assert(before.result == wng::Result::Ok);
        const wng::Graph before_graph = restore_to_fresh_graph(before.snapshot);

        const wng::GraphSnapshot invalid_snapshot =
            make_invalid_snapshot_with_broken_node_port_reference();
        const wng::Result restore_result =
            wng::restore_graph_snapshot(target, invalid_snapshot);

        assert(restore_result != wng::Result::Ok);
        assert_graphs_equal(before_graph, target);
    }

    {
        // Verifies snapshots are useful for command regression workflows without
        // making command history depend on snapshots.
        wng::Graph graph;
        const wng::GraphSnapshotResult baseline =
            wng::capture_graph_snapshot(graph);
        assert(baseline.result == wng::Result::Ok);

        const wng::GraphCommandResult create_result =
            wng::command_create_node(graph, make_node_desc("Command node"));
        assert(create_result.result == wng::Result::Ok);

        const wng::GraphUndoResult undo_result =
            wng::undo_command(graph, create_result.record);
        assert(undo_result.result == wng::Result::Ok);

        const wng::Graph restored_baseline =
            restore_to_fresh_graph(baseline.snapshot);
        assert_graphs_equal(restored_baseline, graph);
    }

    {
        // Verifies restore performs a defensive validation pass after import.
        // The target graph must stay unchanged if imported DTO contents are invalid.
        wng::Graph target;
        build_linked_graph(target);
        const wng::GraphSnapshotResult before =
            wng::capture_graph_snapshot(target);
        assert(before.result == wng::Result::Ok);
        const wng::Graph before_graph = restore_to_fresh_graph(before.snapshot);

        wng::GraphSnapshot invalid_snapshot;
        wng::NodeDto node;
        node.id = wng::NodeId { 1 };
        node.type = "broken.node";
        node.title = "Broken";
        invalid_snapshot.graph.nodes.push_back(node);

        wng::PortDto port;
        port.id = wng::PortId { 1 };
        port.node = wng::NodeId { 999 };
        port.kind = wng::PortKind::Input;
        port.name = "orphan";
        port.type = "number";
        invalid_snapshot.graph.ports.push_back(port);

        const wng::Result restore_result =
            wng::restore_graph_snapshot(target, invalid_snapshot);

        assert(restore_result != wng::Result::Ok);
        assert_graphs_equal(before_graph, target);
    }

    return 0;
}
