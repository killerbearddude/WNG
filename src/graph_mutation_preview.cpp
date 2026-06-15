// Implements deterministic, non-mutating previews for graph removals.
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

    wng::GraphMutationPreviewHostRequest destroy_node_request(wng::NodeId node)
    {
        wng::GraphMutationPreviewHostRequest request;
        request.operation = wng::GraphMutationPreviewOperation::DestroyNode;
        request.node = node;
        return request;
    }

    wng::GraphMutationPreviewHostRequest remove_port_request(wng::PortId port)
    {
        wng::GraphMutationPreviewHostRequest request;
        request.operation = wng::GraphMutationPreviewOperation::RemovePort;
        request.port = port;
        return request;
    }

    wng::GraphMutationPreviewHostRequest destroy_link_request(wng::LinkId link)
    {
        wng::GraphMutationPreviewHostRequest request;
        request.operation = wng::GraphMutationPreviewOperation::DestroyLink;
        request.link = link;
        return request;
    }

    wng::Result append_host_preview(
        const wng::Graph& graph,
        const wng::GraphMutationPreviewHostRequest& request,
        const wng::GraphMutationPreviewOptions& options,
        wng::GraphMutationPreview& preview)
    {
        if (options.callback == nullptr) {
            return wng::Result::Ok;
        }

        try {
            return options.callback->preview_mutation(
                graph,
                request,
                preview,
                preview.host_consequences);
        } catch (const std::bad_alloc&) {
            preview.host_consequences.clear();
            return wng::Result::AllocationFailure;
        }
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
            summary.removed_links.empty() &&
            host_consequences.empty();
    }

    GraphMutationPreview preview_destroy_node(
        const Graph& graph,
        NodeId node)
    {
        return preview_destroy_node(graph, node, GraphMutationPreviewOptions {});
    }

    GraphMutationPreview preview_destroy_node(
        const Graph& graph,
        NodeId node,
        const GraphMutationPreviewOptions& options)
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

            // Link cleanup follows graph link storage order, matching removal
            // summaries and avoiding hidden numeric-ID sorting policy.
            for (const Link& link : graph.links()) {
                if (link_touches_any_port(link, preview.summary.removed_ports)) {
                    preview.summary.removed_links.push_back(link.id);
                }
            }

            const Result host_result = append_host_preview(
                graph,
                destroy_node_request(node),
                options,
                preview);
            if (host_result != Result::Ok) {
                return preview_failure(host_result);
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
        return preview_remove_port(graph, port, GraphMutationPreviewOptions {});
    }

    GraphMutationPreview preview_remove_port(
        const Graph& graph,
        PortId port,
        const GraphMutationPreviewOptions& options)
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

            const Result host_result = append_host_preview(
                graph,
                remove_port_request(port),
                options,
                preview);
            if (host_result != Result::Ok) {
                return preview_failure(host_result);
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
        return preview_destroy_link(graph, link, GraphMutationPreviewOptions {});
    }

    GraphMutationPreview preview_destroy_link(
        const Graph& graph,
        LinkId link,
        const GraphMutationPreviewOptions& options)
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

            const Result host_result = append_host_preview(
                graph,
                destroy_link_request(link),
                options,
                preview);
            if (host_result != Result::Ok) {
                return preview_failure(host_result);
            }

            return preview;
        } catch (const std::bad_alloc&) {
            return preview_failure(Result::AllocationFailure);
        }
    }
}
