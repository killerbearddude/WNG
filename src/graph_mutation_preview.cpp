// Implements deterministic, non-mutating previews for destructive graph mutations.
// Preview summaries are kept in parity with GraphMutationSummary so future editor
// and command systems can trust that previewed cleanup matches actual cleanup.

#include <new>
#include <vector>

#include <wng/graph_mutation_preview.hpp>

#include <wng/graph.hpp>
#include <wng/link.hpp>
#include <wng/node.hpp>
#include <wng/port.hpp>

namespace
{
    bool contains_port_id(
        const std::vector<wng::PortId>& ports,
        wng::PortId id)
    {
        for (wng::PortId port : ports) {
            if (port == id) {
                return true;
            }
        }

        return false;
    }

    bool link_touches_any_port(
        const wng::Link& link,
        const std::vector<wng::PortId>& ports)
    {
        return contains_port_id(ports, link.from) ||
            contains_port_id(ports, link.to);
    }

    wng::Result append_node_ports_in_mutation_order(
        const wng::Graph& graph,
        const wng::Node& node,
        std::vector<wng::PortId>& out_ports)
    {
        // Graph::destroy_node reports inputs first and outputs second using the
        // node-owned port lists. Preview uses the same source of truth rather
        // than sorting by numeric ID.
        for (wng::PortId port : node.inputs) {
            if (graph.find_port(port) == nullptr) {
                return wng::Result::NotFound;
            }
            out_ports.push_back(port);
        }

        for (wng::PortId port : node.outputs) {
            if (graph.find_port(port) == nullptr) {
                return wng::Result::NotFound;
            }
            out_ports.push_back(port);
        }

        return wng::Result::Ok;
    }

    wng::GraphMutationPreview preview_failure(wng::Result result)
    {
        wng::GraphMutationPreview preview;
        preview.result = result;
        return preview;
    }
}

namespace wng
{
    bool GraphMutationPreview::success() const
    {
        return result == Result::Ok;
    }

    bool GraphMutationPreview::empty() const
    {
        return summary.removed_nodes.empty() &&
            summary.removed_ports.empty() &&
            summary.removed_links.empty();
    }

    GraphMutationPreview preview_destroy_node(
        const Graph& graph,
        NodeId node)
    {
        try {
            if (node == NodeId {}) {
                return preview_failure(Result::InvalidArgument);
            }

            const Node* existing_node = graph.find_node(node);
            if (existing_node == nullptr) {
                return preview_failure(Result::NotFound);
            }

            GraphMutationPreview preview;
            preview.summary.removed_nodes.push_back(node);

            const Result port_result = append_node_ports_in_mutation_order(
                graph,
                *existing_node,
                preview.summary.removed_ports);
            if (port_result != Result::Ok) {
                return preview_failure(port_result);
            }

            // Link cleanup follows graph link storage order, matching destructive
            // mutation summaries and avoiding hidden numeric-ID sorting policy.
            for (const Link& link : graph.links()) {
                if (link_touches_any_port(link, preview.summary.removed_ports)) {
                    preview.summary.removed_links.push_back(link.id);
                }
            }

            return preview;
        } catch (const std::bad_alloc&) {
            return preview_failure(Result::AllocationFailure);
        }
    }

    GraphMutationPreview preview_remove_port(
        const Graph& graph,
        PortId port)
    {
        try {
            if (port == PortId {}) {
                return preview_failure(Result::InvalidArgument);
            }

            if (graph.find_port(port) == nullptr) {
                return preview_failure(Result::NotFound);
            }

            GraphMutationPreview preview;
            preview.summary.removed_ports.push_back(port);

            // The preview is a direct graph inspection query. It does not execute
            // commands or mutate a copy of Graph to obtain the summary.
            for (const Link& link : graph.links()) {
                if (link.from == port || link.to == port) {
                    preview.summary.removed_links.push_back(link.id);
                }
            }

            return preview;
        } catch (const std::bad_alloc&) {
            return preview_failure(Result::AllocationFailure);
        }
    }

    GraphMutationPreview preview_destroy_link(
        const Graph& graph,
        LinkId link)
    {
        try {
            if (link == LinkId {}) {
                return preview_failure(Result::InvalidArgument);
            }

            if (graph.find_link(link) == nullptr) {
                return preview_failure(Result::NotFound);
            }

            GraphMutationPreview preview;
            preview.summary.removed_links.push_back(link);
            return preview;
        } catch (const std::bad_alloc&) {
            return preview_failure(Result::AllocationFailure);
        }
    }
}
