// Exercises snapshot diff failure propagation for invalid snapshot inputs.
// Snapshot-to-snapshot diagnostics must fail cleanly without mutating valid
// snapshot DTO values.

#include <cassert>
#include <cstddef>

#include <wng/graph.hpp>
#include <wng/graph_snapshot.hpp>

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

    wng::GraphSnapshot make_snapshot()
    {
        wng::Graph graph;
        wng::NodeId node;
        assert(graph.create_node(make_node_desc("Snapshot node"), &node) ==
            wng::Result::Ok);

        const wng::GraphSnapshotResult snapshot = wng::capture_graph_snapshot(graph);
        assert(snapshot.result == wng::Result::Ok);
        assert(!snapshot.snapshot.empty());
        return snapshot.snapshot;
    }

    wng::GraphSnapshot make_duplicate_node_snapshot()
    {
        wng::GraphSnapshot snapshot = make_snapshot();
        assert(!snapshot.graph.nodes.empty());
        snapshot.graph.nodes.push_back(snapshot.graph.nodes.front());
        return snapshot;
    }

    void assert_snapshot_shape_unchanged(
        const wng::GraphSnapshot& snapshot,
        std::size_t node_count,
        std::size_t port_count,
        std::size_t link_count,
        bool was_empty)
    {
        assert(snapshot.graph.nodes.size() == node_count);
        assert(snapshot.graph.ports.size() == port_count);
        assert(snapshot.graph.links.size() == link_count);
        assert(snapshot.empty() == was_empty);
    }
}

int main()
{
    {
        // Invalid before snapshots should fail before diffing and leave the valid
        // after snapshot DTO untouched.
        const wng::GraphSnapshot invalid_before = make_duplicate_node_snapshot();
        const wng::GraphSnapshot valid_after = make_snapshot();
        const std::size_t node_count = valid_after.graph.nodes.size();
        const std::size_t port_count = valid_after.graph.ports.size();
        const std::size_t link_count = valid_after.graph.links.size();
        const bool was_empty = valid_after.empty();

        const wng::GraphDiff diff =
            wng::diff_graph_snapshots(invalid_before, valid_after);

        assert(diff.result == wng::Result::AlreadyExists);
        assert(diff.empty());
        assert_snapshot_shape_unchanged(
            valid_after,
            node_count,
            port_count,
            link_count,
            was_empty);
    }

    {
        // Invalid after snapshots should fail after restoring the valid before
        // snapshot into a temporary graph, still without mutating the before value.
        const wng::GraphSnapshot valid_before = make_snapshot();
        const wng::GraphSnapshot invalid_after = make_duplicate_node_snapshot();
        const std::size_t node_count = valid_before.graph.nodes.size();
        const std::size_t port_count = valid_before.graph.ports.size();
        const std::size_t link_count = valid_before.graph.links.size();
        const bool was_empty = valid_before.empty();

        const wng::GraphDiff diff =
            wng::diff_graph_snapshots(valid_before, invalid_after);

        assert(diff.result == wng::Result::AlreadyExists);
        assert(diff.empty());
        assert_snapshot_shape_unchanged(
            valid_before,
            node_count,
            port_count,
            link_count,
            was_empty);
    }

    return 0;
}
