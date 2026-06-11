// Compares WNG graphs by stable graph object identity.
// The diff layer is deterministic and non-mutating; it exists for diagnostics,
// undo/redo verification, import/export tests, and future editor tooling.

#pragma once

#include <vector>

#include <wng/graph.hpp>
#include <wng/link.hpp>
#include <wng/node.hpp>
#include <wng/port.hpp>
#include <wng/result.hpp>

namespace wng
{
    // Describes whether a graph object was added, removed, or modified between
    // two graph snapshots.
    enum class GraphDiffChange {
        Added,
        Removed,
        Modified
    };

    // Captures a node-level change. For added nodes, before is default-valued;
    // for removed nodes, after is default-valued; modified nodes carry both.
    struct NodeDiff {
        GraphDiffChange change = GraphDiffChange::Modified;
        NodeId id;
        Node before;
        Node after;
    };

    // Captures a port-level change using stable PortId identity.
    struct PortDiff {
        GraphDiffChange change = GraphDiffChange::Modified;
        PortId id;
        Port before;
        Port after;
    };

    // Captures a link-level change using stable LinkId identity.
    struct LinkDiff {
        GraphDiffChange change = GraphDiffChange::Modified;
        LinkId id;
        Link before;
        Link after;
    };

    // Aggregates all object-level graph differences. The result code reports
    // whether validation and comparison succeeded before any diff entries are used.
    struct GraphDiff {
        Result result = Result::Ok;

        std::vector<NodeDiff> nodes;
        std::vector<PortDiff> ports;
        std::vector<LinkDiff> links;

        bool empty() const;
        bool changed() const;
        bool success() const;
    };

    // Computes a deterministic structural diff between two valid graphs.
    // Object identity is based on stable NodeId, PortId, and LinkId values.
    GraphDiff diff_graphs(
        const Graph& before,
        const Graph& after);
}
