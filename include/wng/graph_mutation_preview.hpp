// Computes non-mutating previews for graph removals.
// The preview layer mirrors GraphMutationSummary ordering so editor,
// transaction, and command systems can inspect consequences before mutation.

#pragma once

#include <string>
#include <vector>

#include <wng/ids.hpp>
#include <wng/mutation_summary.hpp>
#include <wng/result.hpp>

namespace wng
{
    class Graph;

    enum class GraphMutationPreviewOperation {
        DestroyNode,
        RemovePort,
        DestroyLink
    };

    struct GraphMutationPreviewHostConsequence {
        Result result = Result::Ok;
        GraphMutationPreviewOperation operation = GraphMutationPreviewOperation::DestroyNode;
        NodeId node;
        PortId port;
        LinkId link;
        std::string message;
    };

    struct GraphMutationPreviewHostRequest {
        GraphMutationPreviewOperation operation = GraphMutationPreviewOperation::DestroyNode;
        NodeId node;
        PortId port;
        LinkId link;
    };

    // Reports the objects that a destructive graph mutation would remove. The
    // result is a preview only; no command is executed and Graph is not mutated.
    struct GraphMutationPreview {
        Result result = Result::Ok;
        GraphMutationSummary summary;
        std::vector<GraphMutationPreviewHostConsequence> host_consequences;

        bool success() const;
        bool empty() const;
    };

    class GraphMutationPreviewCallback {
    public:
        virtual ~GraphMutationPreviewCallback() = default;

        virtual Result preview_mutation(
            const Graph& graph,
            const GraphMutationPreviewHostRequest& request,
            const GraphMutationPreview& core_preview,
            std::vector<GraphMutationPreviewHostConsequence>& out_consequences) const = 0;
    };

    struct GraphMutationPreviewOptions {
        const GraphMutationPreviewCallback* callback = nullptr;
    };

    // Previews the objects that would be removed by Graph::destroy_node.
    // The graph is not mutated; ordering must match the actual destroy_node summary.
    GraphMutationPreview preview_destroy_node(
        const Graph& graph,
        NodeId node);

    GraphMutationPreview preview_destroy_node(
        const Graph& graph,
        NodeId node,
        const GraphMutationPreviewOptions& options);

    // Previews the objects that would be removed by Graph::remove_port.
    // The graph is not mutated; ordering must match the actual remove_port summary.
    GraphMutationPreview preview_remove_port(
        const Graph& graph,
        PortId port);

    GraphMutationPreview preview_remove_port(
        const Graph& graph,
        PortId port,
        const GraphMutationPreviewOptions& options);

    // Previews the objects that would be removed by Graph::destroy_link.
    // The graph is not mutated; ordering must match the actual destroy_link summary.
    GraphMutationPreview preview_destroy_link(
        const Graph& graph,
        LinkId link);

    GraphMutationPreview preview_destroy_link(
        const Graph& graph,
        LinkId link,
        const GraphMutationPreviewOptions& options);
}
