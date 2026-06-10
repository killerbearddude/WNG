// Provides deterministic dirty propagation analysis for WNG graphs.
// This layer computes affected node sets from graph changes, but it does not
// store dirty state in Graph and does not execute node behavior.

#pragma once

#include <vector>

#include <wng/ids.hpp>
#include <wng/result.hpp>

namespace wng
{
    class Graph;

    struct DirtyPropagationRequest {
        std::vector<NodeId> changed_nodes;
        std::vector<PortId> changed_ports;
        std::vector<LinkId> changed_links;

        bool include_sources = true;
    };

    struct DirtyPropagationResult {
        Result result = Result::Ok;

        std::vector<NodeId> source_nodes;
        std::vector<NodeId> dirty_nodes;
        std::vector<NodeId> ordered_dirty_nodes;
        std::vector<NodeId> unresolved_nodes;

        bool success() const;
        bool complete() const;
    };

    // Computes downstream dirty nodes from changed graph objects.
    // The result is deterministic: source nodes follow request order with
    // duplicates removed, dirty nodes follow traversal order, and ordered nodes
    // follow topological order when the affected subgraph is acyclic.
    DirtyPropagationResult propagate_dirty(
        const Graph& graph,
        const DirtyPropagationRequest& request);
}
