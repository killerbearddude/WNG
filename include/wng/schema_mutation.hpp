// Provides schema-aware graph mutation helpers for WNG.
// These helpers layer schema policy over Graph without making Graph own or
// depend on GraphSchema.

#pragma once

#include <wng/graph.hpp>
#include <wng/result.hpp>
#include <wng/schema.hpp>

namespace wng
{
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
