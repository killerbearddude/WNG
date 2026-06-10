// Provides schema-aware validation helpers for WNG graph connections.
// This layer composes built-in structural validation with GraphSchema rules;
// it does not mutate graphs and does not perform host callback validation.

#pragma once

#include <wng/graph.hpp>
#include <wng/schema.hpp>
#include <wng/validation.hpp>

namespace wng
{
    // Validates a proposed connection against core graph rules first, then
    // against the supplied schema. Built-in rejection is final: schema rules
    // may only reject an otherwise valid built-in connection.
    ConnectionValidation validate_connection(
        const Graph& graph,
        const GraphSchema& schema,
        PortId from,
        PortId to);
}
