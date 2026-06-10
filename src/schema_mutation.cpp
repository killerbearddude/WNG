// Implements schema-aware mutation helpers that remain outside Graph.
// Keeping these helpers separate preserves Graph as a small structural model
// and lets callers opt into schema policy only when they have a GraphSchema.

#include <string>
#include <vector>

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

    bool contains_duplicate_port_name(const std::vector<wng::PortDefinition>& definitions)
    {
        for (std::vector<wng::PortDefinition>::size_type i = 0; i < definitions.size(); ++i) {
            for (std::vector<wng::PortDefinition>::size_type j = i + 1; j < definitions.size(); ++j) {
                if (definitions[i].name == definitions[j].name) {
                    return true;
                }
            }
        }

        return false;
    }

    wng::Result validate_port_definitions(
        const std::vector<wng::PortDefinition>& definitions,
        wng::PortKind required_kind)
    {
        for (const wng::PortDefinition& definition : definitions) {
            if (!is_valid_port_kind(definition.kind) || definition.kind != required_kind) {
                return wng::Result::InvalidArgument;
            }

            if (!definition.enabled) {
                return wng::Result::InvalidConnection;
            }
        }

        if (contains_duplicate_port_name(definitions)) {
            return wng::Result::AlreadyExists;
        }

        return wng::Result::Ok;
    }

    wng::Result validate_instantiable_definition(const wng::NodeDefinition& definition)
    {
        if (!definition.enabled) {
            return wng::Result::InvalidConnection;
        }

        // Validate the full schema definition before mutating Graph so malformed
        // definitions cannot leave behind a partially-instantiated node.
        const wng::Result input_result =
            validate_port_definitions(definition.inputs, wng::PortKind::Input);
        if (input_result != wng::Result::Ok) {
            return input_result;
        }

        return validate_port_definitions(definition.outputs, wng::PortKind::Output);
    }

    wng::PortDesc make_port_desc(const wng::PortDefinition& definition)
    {
        wng::PortDesc desc;
        desc.kind = definition.kind;
        desc.name = definition.name;
        desc.type = definition.type;
        desc.visible = definition.visible;
        desc.enabled = definition.enabled;
        return desc;
    }

    wng::Result add_definition_ports(
        wng::Graph& graph,
        const wng::GraphSchema& schema,
        wng::NodeId node,
        const std::vector<wng::PortDefinition>& definitions)
    {
        for (const wng::PortDefinition& definition : definitions) {
            wng::PortId port;
            const wng::Result result =
                wng::add_port(graph, schema, node, make_port_desc(definition), &port);
            if (result != wng::Result::Ok) {
                return result;
            }
        }

        return wng::Result::Ok;
    }

    void publish_rollback_summary(
        const wng::GraphMutationSummary& rollback,
        wng::GraphMutationSummary* out_rollback_summary)
    {
        if (out_rollback_summary != nullptr) {
            *out_rollback_summary = rollback;
        }
    }

    wng::Result rollback_instantiated_node(
        wng::Graph& graph,
        wng::NodeId created_node,
        wng::Result original_result,
        wng::GraphMutationSummary* out_rollback_summary)
    {
        wng::GraphMutationSummary rollback;
        const wng::Result rollback_result = graph.destroy_node(created_node, &rollback);
        if (rollback_result != wng::Result::Ok) {
            return rollback_result;
        }

        // Rollback details are published only after partial graph state is removed.
        // This makes the summary meaningful only for post-node-creation failures.
        publish_rollback_summary(rollback, out_rollback_summary);
        return original_result;
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

    Result instantiate_node(
        Graph& graph,
        const GraphSchema& schema,
        const NodeDesc& desc,
        NodeId* out_id,
        GraphMutationSummary* out_rollback_summary)
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

        const Result definition_result = validate_instantiable_definition(*definition);
        if (definition_result != Result::Ok) {
            return definition_result;
        }

        NodeId created_node;
        const Result node_result = create_node(graph, schema, desc, &created_node);
        if (node_result != Result::Ok) {
            return node_result;
        }

        const Result input_result =
            add_definition_ports(graph, schema, created_node, definition->inputs);
        if (input_result != Result::Ok) {
            return rollback_instantiated_node(
                graph,
                created_node,
                input_result,
                out_rollback_summary);
        }

        const Result output_result =
            add_definition_ports(graph, schema, created_node, definition->outputs);
        if (output_result != Result::Ok) {
            return rollback_instantiated_node(
                graph,
                created_node,
                output_result,
                out_rollback_summary);
        }

        *out_id = created_node;
        return Result::Ok;
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
