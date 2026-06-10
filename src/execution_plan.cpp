// Implements deterministic execution plan construction for WNG.
// Planning is intentionally separate from execution: this file builds dependency
// metadata only and never evaluates nodes, stores runtime values, or mutates Graph.

#include <new>
#include <vector>

#include <wng/execution_plan.hpp>

#include <wng/dirty_propagation.hpp>
#include <wng/graph.hpp>
#include <wng/graph_traversal.hpp>
#include <wng/graph_validation.hpp>

namespace
{
    bool contains_node_id(const std::vector<wng::NodeId>& nodes, wng::NodeId id)
    {
        for (wng::NodeId node : nodes) {
            if (node == id) {
                return true;
            }
        }

        return false;
    }

    void append_unique(std::vector<wng::NodeId>& nodes, wng::NodeId id)
    {
        if (!contains_node_id(nodes, id)) {
            nodes.push_back(id);
        }
    }

    wng::Result first_error_result(const wng::ValidationReport& report)
    {
        for (const wng::ValidationIssue& issue : report.issues) {
            if (issue.severity == wng::ValidationSeverity::Error) {
                return issue.result;
            }
        }

        return wng::Result::Ok;
    }

    const wng::Node* node_for_port(const wng::Graph& graph, wng::PortId port)
    {
        const wng::Port* graph_port = graph.find_port(port);
        if (graph_port == nullptr) {
            return nullptr;
        }

        return graph.find_node(graph_port->node);
    }

    std::vector<wng::NodeId> collect_dependencies(
        const wng::Graph& graph,
        wng::NodeId node,
        const std::vector<wng::NodeId>& planned_nodes)
    {
        std::vector<wng::NodeId> dependencies;

        // Dependency metadata is derived from graph.links() storage order. That
        // makes independent dependencies deterministic without adding a scheduler
        // or runtime-specific ordering policy.
        for (const wng::Link& link : graph.links()) {
            const wng::Node* source = node_for_port(graph, link.from);
            const wng::Node* target = node_for_port(graph, link.to);

            if (source == nullptr || target == nullptr) {
                continue;
            }

            if (target->id == node && contains_node_id(planned_nodes, source->id)) {
                append_unique(dependencies, source->id);
            }
        }

        return dependencies;
    }

    std::vector<wng::NodeId> collect_dependents(
        const wng::Graph& graph,
        wng::NodeId node,
        const std::vector<wng::NodeId>& planned_nodes)
    {
        std::vector<wng::NodeId> dependents;

        // Dependents use the same link-storage ordering as dependencies so plan
        // metadata remains reproducible across builds and platforms.
        for (const wng::Link& link : graph.links()) {
            const wng::Node* source = node_for_port(graph, link.from);
            const wng::Node* target = node_for_port(graph, link.to);

            if (source == nullptr || target == nullptr) {
                continue;
            }

            if (source->id == node && contains_node_id(planned_nodes, target->id)) {
                append_unique(dependents, target->id);
            }
        }

        return dependents;
    }

    wng::Result build_steps(
        const wng::Graph& graph,
        const std::vector<wng::NodeId>& planned_nodes,
        std::vector<wng::ExecutionPlanStep>& steps)
    {
        for (wng::NodeId node : planned_nodes) {
            wng::ExecutionPlanStep step;
            step.node = node;
            step.dependencies = collect_dependencies(graph, node, planned_nodes);
            step.dependents = collect_dependents(graph, node, planned_nodes);
            steps.push_back(step);
        }

        return wng::Result::Ok;
    }

    wng::ExecutionPlan allocation_failure_plan(wng::ExecutionPlanScope scope)
    {
        wng::ExecutionPlan plan;
        plan.scope = scope;
        plan.result = wng::Result::AllocationFailure;
        return plan;
    }

    wng::ExecutionPlan build_whole_graph_plan(const wng::Graph& graph)
    {
        wng::ExecutionPlan plan;
        plan.scope = wng::ExecutionPlanScope::WholeGraph;

        const wng::ValidationReport validation = wng::validate_graph(graph);
        if (!validation.valid()) {
            const wng::Result result = first_error_result(validation);
            plan.result = result == wng::Result::Ok ? wng::Result::InvalidConnection : result;
            return plan;
        }

        // Whole-graph planning consumes graph topological order directly. It is
        // still planning-only: no callbacks, evaluators, values, or execution
        // state are introduced here.
        const wng::TopologicalOrderResult order = wng::topological_sort(graph);
        plan.planned_nodes = order.ordered_nodes;
        plan.unresolved_nodes = order.unresolved_nodes;

        if (order.result != wng::Result::Ok) {
            plan.result = order.result;
            return plan;
        }

        const wng::Result step_result = build_steps(graph, plan.planned_nodes, plan.steps);
        if (step_result != wng::Result::Ok) {
            plan.result = step_result;
            return plan;
        }

        plan.result = wng::Result::Ok;
        return plan;
    }

    wng::ExecutionPlan build_dirty_subgraph_plan(
        const wng::Graph& graph,
        const wng::ExecutionPlanRequest& request)
    {
        wng::DirtyPropagationRequest dirty_request;
        dirty_request.changed_nodes = request.changed_nodes;
        dirty_request.changed_ports = request.changed_ports;
        dirty_request.changed_links = request.changed_links;
        dirty_request.include_sources = request.include_dirty_sources;

        const wng::DirtyPropagationResult dirty = wng::propagate_dirty(graph, dirty_request);

        wng::ExecutionPlan plan;
        plan.scope = wng::ExecutionPlanScope::DirtySubgraph;
        plan.source_nodes = dirty.source_nodes;
        plan.planned_nodes = dirty.ordered_dirty_nodes;
        plan.unresolved_nodes = dirty.unresolved_nodes;

        if (dirty.result != wng::Result::Ok) {
            plan.result = dirty.result;
            return plan;
        }

        // Dirty-subgraph planning is layered on dirty propagation so it does not
        // duplicate change analysis. The plan merely annotates ordered dirty
        // nodes with dependencies and dependents that are also planned.
        const wng::Result step_result = build_steps(graph, plan.planned_nodes, plan.steps);
        if (step_result != wng::Result::Ok) {
            plan.result = step_result;
            return plan;
        }

        plan.result = wng::Result::Ok;
        return plan;
    }
}

namespace wng
{
    bool ExecutionPlan::success() const
    {
        return result == Result::Ok;
    }

    bool ExecutionPlan::complete() const
    {
        return result == Result::Ok && unresolved_nodes.empty();
    }

    ExecutionPlan build_execution_plan(
        const Graph& graph,
        const ExecutionPlanRequest& request)
    {
        try {
            if (request.scope == ExecutionPlanScope::DirtySubgraph) {
                return build_dirty_subgraph_plan(graph, request);
            }

            // Dirty vectors are intentionally ignored in whole-graph mode. The
            // scope selects either full topology planning or dirty-subgraph
            // planning; mixing semantics here would make plans harder to reason about.
            return build_whole_graph_plan(graph);
        } catch (const std::bad_alloc&) {
            return allocation_failure_plan(request.scope);
        }
    }
}
