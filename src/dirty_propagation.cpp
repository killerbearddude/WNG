// Implements deterministic dirty propagation analysis for WNG graphs.
// Dirty propagation is deliberately non-mutating: it derives affected nodes
// from the current graph state without storing dirty flags in Graph.

#include <new>
#include <vector>

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

    wng::DirtyPropagationResult dirty_failure(wng::Result result)
    {
        wng::DirtyPropagationResult propagation;
        propagation.result = result;
        return propagation;
    }

    wng::Result append_source_from_node(
        const wng::Graph& graph,
        wng::NodeId id,
        std::vector<wng::NodeId>& source_nodes)
    {
        if (id == wng::NodeId {}) {
            return wng::Result::InvalidArgument;
        }

        if (graph.find_node(id) == nullptr) {
            return wng::Result::NotFound;
        }

        append_unique(source_nodes, id);
        return wng::Result::Ok;
    }

    wng::Result append_source_from_port(
        const wng::Graph& graph,
        wng::PortId id,
        std::vector<wng::NodeId>& source_nodes)
    {
        if (id == wng::PortId {}) {
            return wng::Result::InvalidArgument;
        }

        const wng::Port* port = graph.find_port(id);
        if (port == nullptr) {
            return wng::Result::NotFound;
        }

        append_unique(source_nodes, port->node);
        return wng::Result::Ok;
    }

    wng::Result append_source_from_link(
        const wng::Graph& graph,
        wng::LinkId id,
        std::vector<wng::NodeId>& source_nodes)
    {
        if (id == wng::LinkId {}) {
            return wng::Result::InvalidArgument;
        }

        const wng::Link* link = graph.find_link(id);
        if (link == nullptr) {
            return wng::Result::NotFound;
        }

        const wng::Port* source_port = graph.find_port(link->from);
        if (source_port == nullptr) {
            return wng::Result::NotFound;
        }

        append_unique(source_nodes, source_port->node);
        return wng::Result::Ok;
    }

    std::vector<wng::NodeId> filter_to_dirty_nodes(
        const std::vector<wng::NodeId>& ordered,
        const std::vector<wng::NodeId>& dirty)
    {
        std::vector<wng::NodeId> filtered;
        for (wng::NodeId node : ordered) {
            if (contains_node_id(dirty, node)) {
                filtered.push_back(node);
            }
        }

        return filtered;
    }

    wng::Result append_sources_from_request(
        const wng::Graph& graph,
        const wng::DirtyPropagationRequest& request,
        std::vector<wng::NodeId>& source_nodes)
    {
        // Source resolution order is part of the API contract: explicit node
        // changes first, then changed ports, then changed links. This preserves
        // caller intent while still deduplicating repeated source nodes.
        for (wng::NodeId node : request.changed_nodes) {
            const wng::Result result = append_source_from_node(graph, node, source_nodes);
            if (result != wng::Result::Ok) {
                return result;
            }
        }

        for (wng::PortId port : request.changed_ports) {
            const wng::Result result = append_source_from_port(graph, port, source_nodes);
            if (result != wng::Result::Ok) {
                return result;
            }
        }

        for (wng::LinkId link : request.changed_links) {
            const wng::Result result = append_source_from_link(graph, link, source_nodes);
            if (result != wng::Result::Ok) {
                return result;
            }
        }

        return wng::Result::Ok;
    }

    wng::Result append_downstream_dirty_nodes(
        const wng::Graph& graph,
        const std::vector<wng::NodeId>& source_nodes,
        std::vector<wng::NodeId>& dirty_nodes)
    {
        for (wng::NodeId source : source_nodes) {
            const wng::NodeTraversalResult reachable =
                wng::collect_reachable_nodes(graph, source, wng::TraversalDirection::Downstream);

            if (!reachable.success()) {
                return reachable.result;
            }

            for (wng::NodeId node : reachable.nodes) {
                append_unique(dirty_nodes, node);
            }
        }

        return wng::Result::Ok;
    }

    void order_dirty_nodes(
        const wng::Graph& graph,
        wng::DirtyPropagationResult& propagation)
    {
        // Dirty propagation uses traversal to discover affected nodes, then uses
        // topological order to provide deterministic consideration order. The
        // graph remains unchanged; callers decide whether and how to store dirty
        // flags in higher-level systems.
        const wng::TopologicalOrderResult order = wng::topological_sort(graph);

        propagation.ordered_dirty_nodes =
            filter_to_dirty_nodes(order.ordered_nodes, propagation.dirty_nodes);
        propagation.unresolved_nodes =
            filter_to_dirty_nodes(order.unresolved_nodes, propagation.dirty_nodes);

        if (order.result != wng::Result::Ok) {
            propagation.result = order.result;
        }
    }
}

namespace wng
{
    bool DirtyPropagationResult::success() const
    {
        return result == Result::Ok;
    }

    bool DirtyPropagationResult::complete() const
    {
        return result == Result::Ok && unresolved_nodes.empty();
    }

    DirtyPropagationResult propagate_dirty(
        const Graph& graph,
        const DirtyPropagationRequest& request)
    {
        try {
            const ValidationReport validation = validate_graph(graph);
            if (validation.has_errors()) {
                const Result result = first_error_result(validation);
                return dirty_failure(result == Result::Ok ? Result::InvalidConnection : result);
            }

            DirtyPropagationResult propagation;
            const Result source_result =
                append_sources_from_request(graph, request, propagation.source_nodes);
            if (source_result != Result::Ok) {
                return dirty_failure(source_result);
            }

            if (request.include_sources) {
                for (NodeId source : propagation.source_nodes) {
                    append_unique(propagation.dirty_nodes, source);
                }
            }

            const Result downstream_result =
                append_downstream_dirty_nodes(graph, propagation.source_nodes, propagation.dirty_nodes);
            if (downstream_result != Result::Ok) {
                return dirty_failure(downstream_result);
            }

            order_dirty_nodes(graph, propagation);
            return propagation;
        } catch (const std::bad_alloc&) {
            return dirty_failure(Result::AllocationFailure);
        }
    }
}
