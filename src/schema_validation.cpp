// Implements schema-aware connection validation without mutating Graph.
// Built-in validation is deliberately evaluated first so schema rules cannot
// weaken core structural safety.

#include <string>

#include <wng/schema_validation.hpp>

namespace
{
    bool type_compatible(const std::string& graph_type, const std::string& schema_type)
    {
        return graph_type == schema_type ||
               graph_type.empty() ||
               schema_type.empty() ||
               graph_type == "any" ||
               schema_type == "any";
    }

    const wng::PortDefinition* find_output_definition(
        const wng::NodeDefinition& node,
        const wng::Port& port)
    {
        for (const wng::PortDefinition& definition : node.outputs) {
            if (definition.kind == wng::PortKind::Output && definition.name == port.name) {
                return &definition;
            }
        }

        return nullptr;
    }

    const wng::PortDefinition* find_input_definition(
        const wng::NodeDefinition& node,
        const wng::Port& port)
    {
        for (const wng::PortDefinition& definition : node.inputs) {
            if (definition.kind == wng::PortKind::Input && definition.name == port.name) {
                return &definition;
            }
        }

        return nullptr;
    }

    wng::ConnectionValidation allow()
    {
        wng::ConnectionValidation validation;
        validation.status = wng::ConnectionStatus::Allowed;
        validation.result = wng::Result::Ok;
        return validation;
    }

    wng::ConnectionValidation reject(wng::Result result)
    {
        wng::ConnectionValidation validation;
        validation.status = wng::ConnectionStatus::Rejected;
        validation.result = result;
        return validation;
    }

    wng::ConnectionValidation apply_callback(
        const wng::Graph& graph,
        const wng::GraphSchema& schema,
        wng::PortId from,
        wng::PortId to,
        const wng::SchemaValidationOptions& options)
    {
        if (options.callback == nullptr) {
            return allow();
        }

        return options.callback->validate_connection(graph, schema, from, to);
    }
}

namespace wng
{
    ConnectionValidation validate_connection(
        const Graph& graph,
        const GraphSchema& schema,
        PortId from,
        PortId to)
    {
        return validate_connection(
            graph,
            schema,
            from,
            to,
            SchemaValidationOptions {});
    }

    ConnectionValidation validate_connection(
        const Graph& graph,
        const GraphSchema& schema,
        PortId from,
        PortId to,
        const SchemaValidationOptions& options)
    {
        // Built-in graph validation owns structural safety. Schema validation
        // intentionally runs only after that succeeds so schemas cannot permit
        // links that the graph core has rejected.
        const ConnectionValidation built_in = graph.validate_connection(from, to);
        if (built_in.status == ConnectionStatus::Rejected) {
            return built_in;
        }

        const Port* source_port = graph.find_port(from);
        const Port* target_port = graph.find_port(to);
        if (source_port == nullptr || target_port == nullptr) {
            return reject(Result::NotFound);
        }

        const Node* source_node = graph.find_node(source_port->node);
        const Node* target_node = graph.find_node(target_port->node);
        if (source_node == nullptr || target_node == nullptr) {
            return reject(Result::NotFound);
        }

        const NodeDefinition* source_definition = schema.find_node_definition(source_node->type);
        const NodeDefinition* target_definition = schema.find_node_definition(target_node->type);
        if (source_definition == nullptr || target_definition == nullptr) {
            return reject(Result::NotFound);
        }

        if (!source_definition->enabled || !target_definition->enabled) {
            return reject(Result::InvalidConnection);
        }

        const PortDefinition* source_port_definition =
            find_output_definition(*source_definition, *source_port);
        const PortDefinition* target_port_definition =
            find_input_definition(*target_definition, *target_port);

        if (source_port_definition == nullptr || target_port_definition == nullptr) {
            return reject(Result::NotFound);
        }

        if (!source_port_definition->enabled || !target_port_definition->enabled) {
            return reject(Result::InvalidConnection);
        }

        if (!type_compatible(source_port->type, source_port_definition->type)) {
            return reject(Result::InvalidConnection);
        }

        if (!type_compatible(target_port->type, target_port_definition->type)) {
            return reject(Result::InvalidConnection);
        }

        return apply_callback(graph, schema, from, to, options);
    }
}
