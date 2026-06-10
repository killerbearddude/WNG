// Provides schema-aware graph mutation helpers for WNG.
// These helpers layer schema policy over Graph without making Graph own or
// depend on GraphSchema.

#pragma once

#include <wng/graph.hpp>
#include <wng/result.hpp>
#include <wng/schema.hpp>

namespace wng
{
    // Creates a node only if the supplied descriptor references an enabled schema
    // node definition. The Graph remains schema-free; this helper is the opt-in
    // schema-aware creation path for callers that own a GraphSchema.
    Result create_node(
        Graph& graph,
        const GraphSchema& schema,
        const NodeDesc& desc,
        NodeId* out_id);

    // Adds a port only if the target node's schema definition contains a matching
    // enabled port definition. This prevents schema-aware callers from adding ports
    // that the node type does not declare.
    Result add_port(
        Graph& graph,
        const GraphSchema& schema,
        NodeId node,
        const PortDesc& desc,
        PortId* out_id);

    // Creates a link only if both built-in graph validation and schema validation
    // allow the connection. This function does not weaken Graph::create_link;
    // it provides an opt-in schema-aware mutation path for callers that have a schema.
    Result create_link(
        Graph& graph,
        const GraphSchema& schema,
        PortId from,
        PortId to,
        LinkId* out_id);
}
