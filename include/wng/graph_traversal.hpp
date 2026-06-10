// Provides deterministic, non-mutating traversal helpers for WNG graphs.
// Traversal is kept separate from Graph so graph storage and mutation remain
// minimal while higher layers can build dependency analysis and execution plans.

#pragma once

#include <vector>

#include <wng/ids.hpp>
#include <wng/result.hpp>

namespace wng
{
    class Graph;

    enum class TraversalDirection {
        Upstream,
        Downstream
    };

    struct NodeTraversalResult {
        Result result = Result::Ok;
        std::vector<NodeId> nodes;

        bool success() const;
    };

    struct TopologicalOrderResult {
        Result result = Result::Ok;
        std::vector<NodeId> ordered_nodes;
        std::vector<NodeId> unresolved_nodes;

        bool complete() const;
    };

    // Collects nodes reachable from start in the requested direction.
    // The start node is not included in the result; callers already know it.
    NodeTraversalResult collect_reachable_nodes(
        const Graph& graph,
        NodeId start,
        TraversalDirection direction);

    // Produces source-before-sink node order for acyclic graphs.
    // Cycles are reported as InvalidConnection with unresolved_nodes populated;
    // this does not make cycles globally invalid for storage or validation.
    TopologicalOrderResult topological_sort(const Graph& graph);
}
