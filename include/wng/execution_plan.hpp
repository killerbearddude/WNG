// Builds deterministic execution plans from validated WNG graph topology.
// The plan describes ordering and dependencies only; it does not execute nodes,
// store runtime values, or call host-provided evaluators.

#pragma once

#include <vector>

#include <wng/ids.hpp>
#include <wng/result.hpp>

namespace wng
{
    class Graph;

    enum class ExecutionPlanScope {
        WholeGraph,
        DirtySubgraph
    };

    struct ExecutionPlanRequest {
        ExecutionPlanScope scope = ExecutionPlanScope::WholeGraph;

        std::vector<NodeId> changed_nodes;
        std::vector<PortId> changed_ports;
        std::vector<LinkId> changed_links;

        bool include_dirty_sources = true;
    };

    struct ExecutionPlanStep {
        NodeId node;
        std::vector<NodeId> dependencies;
        std::vector<NodeId> dependents;
    };

    struct ExecutionPlan {
        Result result = Result::Ok;

        ExecutionPlanScope scope = ExecutionPlanScope::WholeGraph;
        std::vector<NodeId> source_nodes;
        std::vector<NodeId> planned_nodes;
        std::vector<NodeId> unresolved_nodes;
        std::vector<ExecutionPlanStep> steps;

        bool success() const;
        bool complete() const;
    };

    // Builds a deterministic, non-executing plan for either the whole graph or
    // the dirty subgraph described by the request. The function validates graph
    // structure and reports cycles through unresolved_nodes rather than mutating Graph.
    ExecutionPlan build_execution_plan(
        const Graph& graph,
        const ExecutionPlanRequest& request);
}
