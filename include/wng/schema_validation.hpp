// Provides schema-aware validation helpers for WNG graph connections.
// This layer composes built-in structural validation with GraphSchema rules;
// it does not mutate graphs and keeps host callback validation opt-in.

#pragma once

#include <wng/graph.hpp>
#include <wng/schema.hpp>
#include <wng/validation.hpp>

namespace wng
{
    // Optional host extension point for domain-specific schema connection rules.
    // The graph core does not own callback lifetime. Implementations must treat
    // Graph and GraphSchema as read-only.
    class SchemaValidationCallback {
    public:
        virtual ~SchemaValidationCallback() = default;

        virtual ConnectionValidation validate_connection(
            const Graph& graph,
            const GraphSchema& schema,
            PortId from,
            PortId to) const = 0;
    };

    struct SchemaValidationOptions {
        const SchemaValidationCallback* callback = nullptr;
    };

    // Validates a proposed connection against core graph rules first, then
    // against the supplied schema. Built-in rejection is final: schema rules
    // may only reject an otherwise valid built-in connection.
    ConnectionValidation validate_connection(
        const Graph& graph,
        const GraphSchema& schema,
        PortId from,
        PortId to);

    // Validates a proposed connection against core graph and schema rules, then
    // gives an optional host callback a final read-only policy decision.
    ConnectionValidation validate_connection(
        const Graph& graph,
        const GraphSchema& schema,
        PortId from,
        PortId to,
        const SchemaValidationOptions& options);
}
