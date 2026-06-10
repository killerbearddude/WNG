// Implements schema-aware mutation helpers that remain outside Graph.
// Keeping these helpers separate preserves Graph as a small structural model
// and lets callers opt into schema policy only when they have a GraphSchema.

#include <wng/schema_mutation.hpp>

#include <wng/schema_validation.hpp>

namespace wng
{
    Result create_link(
        Graph& graph,
        const GraphSchema& schema,
        PortId from,
        PortId to,
        LinkId* out_id)
    {
        if (out_id == nullptr) {
            return Result::InvalidArgument;
        }

        // Schema-aware validation is intentionally performed before mutation.
        // It composes built-in graph validation with schema restrictions, but it
        // cannot permit anything the graph core would reject.
        const ConnectionValidation validation =
            validate_connection(graph, schema, from, to);

        if (validation.status == ConnectionStatus::Rejected) {
            return validation.result;
        }

        // Graph remains the final authority for structural mutation. Revalidating
        // inside Graph::create_link keeps this helper from duplicating mutation rules.
        return graph.create_link(from, to, out_id);
    }
}
