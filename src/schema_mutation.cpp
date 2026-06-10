// Implements schema-aware mutation helpers that remain outside Graph.
// Keeping these helpers separate preserves Graph as a small structural model
// and lets callers opt into schema policy only when they have a GraphSchema.

#include <string>

#include <wng/schema_mutation.hpp>

#include <wng/schema_validation.hpp>

namespace
{
    bool is_valid_port_kind(wng::PortKind kind)
    {
        return kind == wng::PortKind::Input || kind == wng::PortKind::Output;
    }

    bool type_compatible(const std::string& requested_type, const std::string& schema_type)
    {
        return requested_type == schema_type ||
               requested_type.empty() ||
               schema_type.empty() ||
               requested_type == "any" ||
               schema_type == "any";
    }

    const wng::PortDefinition* find_port_definition(
        const wng::NodeDefinition& node_definition,
        const wng::PortDesc& desc)
    {
        const std::vector<wng::PortDefinition>& definitions =
            desc.kind == wng::PortKind::Input ? node_definition.inputs : node_definition.outputs;

        for (const wng::PortDefinition& definition : definitions) {
            if (definition.kind == desc.kind && definition.name == desc.name) {
                return &definition;
            }
        }

        return nullptr;
    }

    bool node_already_has_schema_port(
        const wng::Graph& graph,
        const wng::Node& node,
        const wng::PortDesc& desc)
    {
        const std::vector<wng::PortId>& ports =
            desc.kind == wng::PortKind::Input ? node.inputs : node.outputs;

        for (wng::PortId port_id : ports) {
            const wng::Port* port = graph.find_port(port_id);
            if (port != nullptr && port->kind == desc.kind && port->name == desc.name) {
                return true;
            }
        }

        return false;
    }
}

namespace wng
{
    Result create_node(
        Graph& graph,
        const GraphSchema& schema,
        const NodeDesc& desc,
        NodeId* out_id)
    {
        if (out_id == nullptr) {
            return Result::InvalidArgument;
        }

        if (desc.type.empty()) {
            return Result::InvalidArgument;
        }

        const NodeDefinition* definition = schema.find_node_definition(desc.type);
        if (definition == nullptr) {
            return Result::NotFound;
        }

        if (!definition->enabled) {
            return Result::InvalidConnection;
        }

        // Schema-aware creation validates policy before calling Graph. Graph then
        // remains responsible for structural checks such as geometry and ID allocation.
        return graph.create_node(desc, out_id);
    }

    Result add_port(
        Graph& graph,
        const GraphSchema& schema,
        NodeId node,
        const PortDesc& desc,
        PortId* out_id)
    {
        if (out_id == nullptr) {
            return Result::InvalidArgument;
        }

        if (node == NodeId {}) {
            return Result::InvalidArgument;
        }

        if (!is_valid_port_kind(desc.kind)) {
            return Result::InvalidArgument;
        }

        const Node* graph_node = graph.find_node(node);
        if (graph_node == nullptr) {
            return Result::NotFound;
        }

        const NodeDefinition* node_definition = schema.find_node_definition(graph_node->type);
        if (node_definition == nullptr) {
            return Result::NotFound;
        }

        if (!node_definition->enabled) {
            return Result::InvalidConnection;
        }

        const PortDefinition* port_definition = find_port_definition(*node_definition, desc);
        if (port_definition == nullptr) {
            return Result::NotFound;
        }

        if (!port_definition->enabled) {
            return Result::InvalidConnection;
        }

        if (!type_compatible(desc.type, port_definition->type)) {
            return Result::InvalidConnection;
        }

        // Bare Graph::add_port remains permissive. The schema-aware helper adds
        // the stricter one-port-per-kind/name rule before delegating mutation.
        if (node_already_has_schema_port(graph, *graph_node, desc)) {
            return Result::AlreadyExists;
        }

        // Graph is still the final mutation authority; this preserves its
        // allocation-failure handling and node/port storage invariants.
        return graph.add_port(node, desc, out_id);
    }

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
