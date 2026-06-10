// Implements deterministic, non-mutating graph traversal utilities.
// Traversal validates structure up front, then uses only public Graph accessors
// so traversal state never leaks into graph storage.

#include <new>
#include <vector>

#include <wng/graph_traversal.hpp>

#include <wng/graph.hpp>
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

    wng::Result first_error_result(const wng::ValidationReport& report)
    {
        for (const wng::ValidationIssue& issue : report.issues) {
            if (issue.severity == wng::ValidationSeverity::Error) {
                return issue.result;
            }
        }

        return wng::Result::Ok;
    }

    const wng::Node* node_for_port(const wng::Graph& graph, wng::PortId port_id)
    {
        const wng::Port* port = graph.find_port(port_id);
        if (port == nullptr) {
            return nullptr;
        }

        return graph.find_node(port->node);
    }

    wng::NodeTraversalResult traversal_failure(wng::Result result)
    {
        wng::NodeTraversalResult traversal;
        traversal.result = result;
        return traversal;
    }

    wng::TopologicalOrderResult topological_failure(wng::Result result)
    {
        wng::TopologicalOrderResult traversal;
        traversal.result = result;
        return traversal;
    }

    bool node_owns_port(const wng::Graph& graph, wng::NodeId node, wng::PortId port_id)
    {
        const wng::Port* port = graph.find_port(port_id);
        return port != nullptr && port->node == node;
    }

    void collect_downstream_from_current(
        const wng::Graph& graph,
        wng::NodeId current,
        std::vector<wng::NodeId>& pending,
        std::vector<wng::NodeId>& reachable)
    {
        for (const wng::Link& link : graph.links()) {
            if (!node_owns_port(graph, current, link.from)) {
                continue;
            }

            const wng::Node* target = node_for_port(graph, link.to);
            if (target != nullptr &&
                target->id != current &&
                !contains_node_id(reachable, target->id)) {
                reachable.push_back(target->id);
                pending.push_back(target->id);
            }
        }
    }

    void collect_upstream_from_current(
        const wng::Graph& graph,
        wng::NodeId current,
        std::vector<wng::NodeId>& pending,
        std::vector<wng::NodeId>& reachable)
    {
        for (const wng::Link& link : graph.links()) {
            if (!node_owns_port(graph, current, link.to)) {
                continue;
            }

            const wng::Node* source = node_for_port(graph, link.from);
            if (source != nullptr &&
                source->id != current &&
                !contains_node_id(reachable, source->id)) {
                reachable.push_back(source->id);
                pending.push_back(source->id);
            }
        }
    }

    std::vector<int> make_indegrees(const wng::Graph& graph)
    {
        std::vector<int> indegrees(graph.nodes().size(), 0);

        for (const wng::Link& link : graph.links()) {
            const wng::Node* target = node_for_port(graph, link.to);
            if (target == nullptr) {
                continue;
            }

            for (std::vector<wng::Node>::size_type i = 0; i < graph.nodes().size(); ++i) {
                if (graph.nodes()[i].id == target->id) {
                    ++indegrees[i];
                    break;
                }
            }
        }

        return indegrees;
    }

    int node_index(const wng::Graph& graph, wng::NodeId id)
    {
        for (std::vector<wng::Node>::size_type i = 0; i < graph.nodes().size(); ++i) {
            if (graph.nodes()[i].id == id) {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    void decrement_outgoing_targets(
        const wng::Graph& graph,
        wng::NodeId emitted,
        std::vector<int>& indegrees)
    {
        for (const wng::Link& link : graph.links()) {
            if (!node_owns_port(graph, emitted, link.from)) {
                continue;
            }

            const wng::Node* target = node_for_port(graph, link.to);
            if (target == nullptr) {
                continue;
            }

            const int target_index = node_index(graph, target->id);
            if (target_index >= 0 && indegrees[static_cast<std::vector<int>::size_type>(target_index)] > 0) {
                --indegrees[static_cast<std::vector<int>::size_type>(target_index)];
            }
        }
    }
}

namespace wng
{
    bool NodeTraversalResult::success() const
    {
        return result == Result::Ok;
    }

    bool TopologicalOrderResult::complete() const
    {
        return result == Result::Ok && unresolved_nodes.empty();
    }

    NodeTraversalResult collect_reachable_nodes(
        const Graph& graph,
        NodeId start,
        TraversalDirection direction)
    {
        try {
            if (start == NodeId {}) {
                return traversal_failure(Result::InvalidArgument);
            }

            if (graph.find_node(start) == nullptr) {
                return traversal_failure(Result::NotFound);
            }

            const ValidationReport validation = validate_graph(graph);
            if (validation.has_errors()) {
                const Result result = first_error_result(validation);
                return traversal_failure(result == Result::Ok ? Result::InvalidConnection : result);
            }

            NodeTraversalResult traversal;
            std::vector<NodeId> pending;
            pending.push_back(start);

            // Reachability uses breadth-first traversal. Each expansion scans
            // graph.links() in storage order, giving deterministic output for
            // equal-depth dependents/dependencies.
            for (std::vector<NodeId>::size_type index = 0; index < pending.size(); ++index) {
                const NodeId current = pending[index];

                if (direction == TraversalDirection::Downstream) {
                    collect_downstream_from_current(graph, current, pending, traversal.nodes);
                } else {
                    collect_upstream_from_current(graph, current, pending, traversal.nodes);
                }
            }

            traversal.result = Result::Ok;
            return traversal;
        } catch (const std::bad_alloc&) {
            return traversal_failure(Result::AllocationFailure);
        }
    }

    TopologicalOrderResult topological_sort(const Graph& graph)
    {
        try {
            const ValidationReport validation = validate_graph(graph);
            if (validation.has_errors()) {
                const Result result = first_error_result(validation);
                return topological_failure(result == Result::Ok ? Result::InvalidConnection : result);
            }

            TopologicalOrderResult traversal;
            std::vector<int> indegrees = make_indegrees(graph);
            std::vector<NodeId> emitted;

            // This intentionally uses a simple repeated scan over graph.nodes().
            // Node storage order is the deterministic tie-breaker for independent
            // sources, which is more important than asymptotic performance here.
            while (emitted.size() < graph.nodes().size()) {
                bool made_progress = false;

                for (std::vector<Node>::size_type i = 0; i < graph.nodes().size(); ++i) {
                    const NodeId node = graph.nodes()[i].id;
                    if (contains_node_id(emitted, node) || indegrees[i] != 0) {
                        continue;
                    }

                    emitted.push_back(node);
                    traversal.ordered_nodes.push_back(node);
                    decrement_outgoing_targets(graph, node, indegrees);
                    made_progress = true;
                    break;
                }

                if (!made_progress) {
                    // Cycles are reported only for callers requesting a topological
                    // order. This does not make cycles invalid graph storage and
                    // does not alter whole-graph validation policy.
                    for (const Node& node : graph.nodes()) {
                        if (!contains_node_id(emitted, node.id)) {
                            traversal.unresolved_nodes.push_back(node.id);
                        }
                    }

                    traversal.result = Result::InvalidConnection;
                    return traversal;
                }
            }

            traversal.result = Result::Ok;
            return traversal;
        } catch (const std::bad_alloc&) {
            return topological_failure(Result::AllocationFailure);
        }
    }
}
