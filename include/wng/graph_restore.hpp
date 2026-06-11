// Provides atomic graph object restoration from captured value snapshots.
// This layer exists for future undo/redo integration: it restores stable object
// identities without making normal Graph creation APIs accept explicit IDs.

#pragma once

#include <vector>

#include <wng/graph.hpp>
#include <wng/link.hpp>
#include <wng/node.hpp>
#include <wng/port.hpp>
#include <wng/result.hpp>

namespace wng
{
    // Value snapshot of graph objects that should be restored together.
    // The snapshot is in-memory data only; it is not a file format or replay log.
    struct GraphObjectSnapshot {
        std::vector<Node> nodes;
        std::vector<Port> ports;
        std::vector<Link> links;

        bool empty() const;
    };

    // Result of a restore attempt. On failure, restored_* vectors stay empty
    // because the graph is left unchanged and no partial restore is published.
    struct GraphRestoreResult {
        Result result = Result::Ok;

        std::vector<NodeId> restored_nodes;
        std::vector<PortId> restored_ports;
        std::vector<LinkId> restored_links;

        bool success() const;
    };

    // Restores captured graph objects with their original stable IDs.
    // The operation is atomic from the caller's perspective: on failure, Graph
    // remains unchanged and no partial restored objects are published.
    GraphRestoreResult restore_graph_objects(
        Graph& graph,
        const GraphObjectSnapshot& snapshot);
}
