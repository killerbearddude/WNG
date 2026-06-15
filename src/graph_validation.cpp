// Implements deterministic whole-graph validation for WNG.
// The validator inspects public Graph accessors only; it does not mutate graph
// state, schema state, or ID counters.

#include <cmath>
#include <new>
#include <string>
#include <vector>

#include <wng/graph_validation.hpp>

#include <wng/graph.hpp>
#include <wng/graph_traversal.hpp>
#include <wng/schema.hpp>
#include <wng/schema_validation.hpp>

namespace
{
    bool is_valid_port_kind(wng::PortKind kind)
    {
        return kind == wng::PortKind::Input || kind == wng::PortKind::Output;
    }

    bool is_finite(wng::Vec2 value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    bool has_non_negative_size(wng::Vec2 value)
    {
        return value.x >= 0.0f && value.y >= 0.0f;
    }

    bool type_compatible(const std::string& a, const std::string& b)
    {
        return a == b || a.empty() || b.empty() || a == "any" || b == "any";
    }

    void add_issue(
        wng::ValidationReport& report,
        wng::ValidationIssueCode code,
        wng::Result result,
        wng::NodeId node,
        wng::PortId port,
        wng::LinkId link,
        const char* message)
    {
        wng::ValidationIssue issue;
        issue.severity = wng::ValidationSeverity::Error;
        issue.code = code;
        issue.result = result;
        issue.node = node;
        issue.port = port;
        issue.link = link;
        issue.message = message;
        report.issues.push_back(issue);
    }

    wng::ValidationReport allocation_failure_report()
    {
        wng::ValidationReport report;
        try {
            add_issue(
                report,
                wng::ValidationIssueCode::InvalidNodeId,
                wng::Result::AllocationFailure,
                wng::NodeId {},
                wng::PortId {},
                wng::LinkId {},
                "allocation failed while building validation report");
        } catch (const std::bad_alloc&) {
        }
        return report;
    }

    const wng::Node* find_node(const wng::Graph& graph, wng::NodeId id)
    {
        return graph.find_node(id);
    }

    const wng::Port* find_port(const wng::Graph& graph, wng::PortId id)
    {
        return graph.find_port(id);
    }

    bool duplicate_node_id_before(const wng::Graph& graph, std::vector<wng::Node>::size_type index)
    {
        const wng::NodeId id = graph.nodes()[index].id;
        for (std::vector<wng::Node>::size_type i = 0; i < index; ++i) {
            if (graph.nodes()[i].id == id) {
                return true;
            }
        }
        return false;
    }

    bool duplicate_port_id_before(const wng::Graph& graph, std::vector<wng::Port>::size_type index)
    {
        const wng::PortId id = graph.ports()[index].id;
        for (std::vector<wng::Port>::size_type i = 0; i < index; ++i) {
            if (graph.ports()[i].id == id) {
                return true;
            }
        }
        return false;
    }

    bool duplicate_link_id_before(const wng::Graph& graph, std::vector<wng::Link>::size_type index)
    {
        const wng::LinkId id = graph.links()[index].id;
        for (std::vector<wng::Link>::size_type i = 0; i < index; ++i) {
            if (graph.links()[i].id == id) {
                return true;
            }
        }
        return false;
    }

    bool duplicate_exact_link_before(const wng::Graph& graph, std::vector<wng::Link>::size_type index)
    {
        const wng::Link& link = graph.links()[index];
        for (std::vector<wng::Link>::size_type i = 0; i < index; ++i) {
            if (graph.links()[i].from == link.from && graph.links()[i].to == link.to) {
                return true;
            }
        }
        return false;
    }

    bool input_already_linked_before(const wng::Graph& graph, std::vector<wng::Link>::size_type index)
    {
        const wng::PortId input = graph.links()[index].to;
        for (std::vector<wng::Link>::size_type i = 0; i < index; ++i) {
            if (graph.links()[i].to == input) {
                return true;
            }
        }
        return false;
    }

    void validate_node_list(const wng::Graph& graph, wng::ValidationReport& report)
    {
        for (std::vector<wng::Node>::size_type i = 0; i < graph.nodes().size(); ++i) {
            const wng::Node& node = graph.nodes()[i];

            if (node.id == wng::NodeId {}) {
                add_issue(report, wng::ValidationIssueCode::InvalidNodeId, wng::Result::InvalidArgument, node.id, wng::PortId {}, wng::LinkId {}, "node has an invalid zero id");
            }

            if (duplicate_node_id_before(graph, i)) {
                add_issue(report, wng::ValidationIssueCode::DuplicateNodeId, wng::Result::AlreadyExists, node.id, wng::PortId {}, wng::LinkId {}, "node id is duplicated");
            }

            if (!is_finite(node.position) || !is_finite(node.size) || !has_non_negative_size(node.size)) {
                add_issue(report, wng::ValidationIssueCode::InvalidNodeGeometry, wng::Result::InvalidArgument, node.id, wng::PortId {}, wng::LinkId {}, "node geometry is invalid");
            }
        }
    }

    void validate_port_list(const wng::Graph& graph, wng::ValidationReport& report)
    {
        for (std::vector<wng::Port>::size_type i = 0; i < graph.ports().size(); ++i) {
            const wng::Port& port = graph.ports()[i];

            if (port.id == wng::PortId {}) {
                add_issue(report, wng::ValidationIssueCode::InvalidPortId, wng::Result::InvalidArgument, port.node, port.id, wng::LinkId {}, "port has an invalid zero id");
            }

            if (duplicate_port_id_before(graph, i)) {
                add_issue(report, wng::ValidationIssueCode::DuplicatePortId, wng::Result::AlreadyExists, port.node, port.id, wng::LinkId {}, "port id is duplicated");
            }

            if (!is_valid_port_kind(port.kind)) {
                add_issue(report, wng::ValidationIssueCode::InvalidPortKind, wng::Result::InvalidArgument, port.node, port.id, wng::LinkId {}, "port kind is invalid");
            }

            if (find_node(graph, port.node) == nullptr) {
                add_issue(report, wng::ValidationIssueCode::PortOwnedByMissingNode, wng::Result::NotFound, port.node, port.id, wng::LinkId {}, "port owner node is missing");
            }
        }
    }

    void validate_node_port_reference(
        const wng::Graph& graph,
        wng::ValidationReport& report,
        const wng::Node& node,
        wng::PortId port_id,
        wng::PortKind expected_kind)
    {
        const wng::Port* port = find_port(graph, port_id);
        if (port == nullptr) {
            add_issue(report, wng::ValidationIssueCode::NodeReferencesMissingPort, wng::Result::NotFound, node.id, port_id, wng::LinkId {}, "node references a missing port");
            return;
        }

        if (port->node != node.id) {
            add_issue(report, wng::ValidationIssueCode::NodeReferencesForeignPort, wng::Result::InvalidConnection, node.id, port_id, wng::LinkId {}, "node references a port owned by another node");
        }

        if (port->kind != expected_kind) {
            add_issue(report, wng::ValidationIssueCode::NodeReferencesWrongPortDirection, wng::Result::InvalidConnection, node.id, port_id, wng::LinkId {}, "node references a port with the wrong direction");
        }
    }

    void validate_node_owned_port_references(const wng::Graph& graph, wng::ValidationReport& report)
    {
        for (const wng::Node& node : graph.nodes()) {
            for (wng::PortId port_id : node.inputs) {
                validate_node_port_reference(graph, report, node, port_id, wng::PortKind::Input);
            }

            for (wng::PortId port_id : node.outputs) {
                validate_node_port_reference(graph, report, node, port_id, wng::PortKind::Output);
            }
        }
    }

    void validate_links(const wng::Graph& graph, wng::ValidationReport& report)
    {
        for (std::vector<wng::Link>::size_type i = 0; i < graph.links().size(); ++i) {
            const wng::Link& link = graph.links()[i];

            if (link.id == wng::LinkId {}) {
                add_issue(report, wng::ValidationIssueCode::InvalidLinkId, wng::Result::InvalidArgument, wng::NodeId {}, wng::PortId {}, link.id, "link has an invalid zero id");
            }

            if (duplicate_link_id_before(graph, i)) {
                add_issue(report, wng::ValidationIssueCode::DuplicateLinkId, wng::Result::AlreadyExists, wng::NodeId {}, wng::PortId {}, link.id, "link id is duplicated");
            }

            const wng::Port* from = find_port(graph, link.from);
            const wng::Port* to = find_port(graph, link.to);

            if (from == nullptr) {
                add_issue(report, wng::ValidationIssueCode::LinkReferencesMissingPort, wng::Result::NotFound, wng::NodeId {}, link.from, link.id, "link source port is missing");
            }

            if (to == nullptr) {
                add_issue(report, wng::ValidationIssueCode::LinkReferencesMissingPort, wng::Result::NotFound, wng::NodeId {}, link.to, link.id, "link target port is missing");
            }

            if (from == nullptr || to == nullptr) {
                continue;
            }

            if (from->kind != wng::PortKind::Output || to->kind != wng::PortKind::Input) {
                add_issue(report, wng::ValidationIssueCode::LinkDirectionMismatch, wng::Result::InvalidConnection, wng::NodeId {}, wng::PortId {}, link.id, "link direction is invalid");
            }

            if (from->node == to->node) {
                add_issue(report, wng::ValidationIssueCode::LinkSameNode, wng::Result::InvalidConnection, from->node, wng::PortId {}, link.id, "link connects ports on the same node");
            }

            if (duplicate_exact_link_before(graph, i)) {
                add_issue(report, wng::ValidationIssueCode::LinkDuplicate, wng::Result::AlreadyExists, wng::NodeId {}, wng::PortId {}, link.id, "link duplicates an earlier exact link");
            }

            if (input_already_linked_before(graph, i)) {
                add_issue(report, wng::ValidationIssueCode::LinkMultipleIntoInput, wng::Result::InvalidConnection, to->node, to->id, link.id, "input port already has an incoming link");
            }

            if (!type_compatible(from->type, to->type)) {
                add_issue(report, wng::ValidationIssueCode::LinkTypeMismatch, wng::Result::InvalidConnection, wng::NodeId {}, wng::PortId {}, link.id, "link endpoint types are incompatible");
            }
        }
    }

    void validate_acyclic_mode(
        const wng::Graph& graph,
        const wng::GraphValidationOptions& options,
        wng::ValidationReport& report)
    {
        if (options.cycle_mode != wng::GraphCycleMode::RequireAcyclic || report.has_errors()) {
            return;
        }

        const wng::TopologicalOrderResult order = wng::topological_sort(graph);
        if (order.result == wng::Result::Ok) {
            return;
        }

        if (order.result == wng::Result::AllocationFailure) {
            add_issue(
                report,
                wng::ValidationIssueCode::InvalidNodeId,
                wng::Result::AllocationFailure,
                wng::NodeId {},
                wng::PortId {},
                wng::LinkId {},
                "allocation failed while checking graph cycles");
            return;
        }

        if (order.unresolved_nodes.empty()) {
            add_issue(
                report,
                wng::ValidationIssueCode::CycleDetected,
                wng::Result::InvalidConnection,
                wng::NodeId {},
                wng::PortId {},
                wng::LinkId {},
                "graph contains a cycle");
            return;
        }

        // Reuse topological_sort's deterministic unresolved node ordering. One
        // issue per unresolved node gives diagnostics stable anchors without
        // requiring a separate cycle path extraction algorithm.
        for (wng::NodeId node : order.unresolved_nodes) {
            add_issue(
                report,
                wng::ValidationIssueCode::CycleDetected,
                wng::Result::InvalidConnection,
                node,
                wng::PortId {},
                wng::LinkId {},
                "graph contains a cycle");
        }
    }

    void append_host_validation(
        const wng::Graph& graph,
        const wng::GraphValidationOptions& options,
        wng::ValidationReport& report)
    {
        if (options.callback == nullptr) {
            return;
        }

        const wng::Result result = options.callback->validate_graph(graph, report);
        if (result != wng::Result::Ok) {
            add_issue(
                report,
                wng::ValidationIssueCode::HostValidationIssue,
                result,
                wng::NodeId {},
                wng::PortId {},
                wng::LinkId {},
                "host validation callback failed");
        }
    }

    const wng::PortDefinition* find_port_definition(
        const wng::NodeDefinition& node_definition,
        const wng::Port& port)
    {
        const std::vector<wng::PortDefinition>& definitions =
            port.kind == wng::PortKind::Input ? node_definition.inputs : node_definition.outputs;

        for (const wng::PortDefinition& definition : definitions) {
            if (definition.kind == port.kind && definition.name == port.name) {
                return &definition;
            }
        }

        return nullptr;
    }

    bool node_has_required_port(
        const wng::Graph& graph,
        const wng::Node& node,
        const wng::PortDefinition& required_port)
    {
        for (const wng::Port& port : graph.ports()) {
            if (port.node == node.id &&
                port.kind == required_port.kind &&
                port.name == required_port.name &&
                type_compatible(port.type, required_port.type)) {
                return true;
            }
        }

        return false;
    }

    void validate_schema_ports_for_node(
        const wng::Graph& graph,
        const wng::Node& node,
        const wng::NodeDefinition& node_definition,
        wng::ValidationReport& report)
    {
        for (const wng::Port& port : graph.ports()) {
            if (port.node != node.id) {
                continue;
            }

            const wng::PortDefinition* port_definition = find_port_definition(node_definition, port);
            if (port_definition == nullptr) {
                add_issue(report, wng::ValidationIssueCode::MissingPortDefinition, wng::Result::NotFound, node.id, port.id, wng::LinkId {}, "schema does not define this graph port");
                continue;
            }

            if (!port_definition->enabled) {
                add_issue(report, wng::ValidationIssueCode::DisabledPortDefinition, wng::Result::InvalidConnection, node.id, port.id, wng::LinkId {}, "schema port definition is disabled");
            }

            if (!type_compatible(port.type, port_definition->type)) {
                add_issue(report, wng::ValidationIssueCode::PortTypeMismatch, wng::Result::InvalidConnection, node.id, port.id, wng::LinkId {}, "graph port type does not match schema port type");
            }
        }
    }

    void validate_required_ports(
        const wng::Graph& graph,
        const wng::Node& node,
        const std::vector<wng::PortDefinition>& definitions,
        wng::ValidationReport& report)
    {
        for (const wng::PortDefinition& definition : definitions) {
            if (!definition.required) {
                continue;
            }

            if (!node_has_required_port(graph, node, definition)) {
                add_issue(report, wng::ValidationIssueCode::RequiredPortMissing, wng::Result::InvalidConnection, node.id, wng::PortId {}, wng::LinkId {}, "required schema port is missing");
            }
        }
    }

    void validate_against_schema(
        const wng::Graph& graph,
        const wng::GraphSchema& schema,
        wng::ValidationReport& report)
    {
        // Schema validation extends structural validation. It appends diagnostics
        // in node order and never suppresses lower-level graph issues.
        for (const wng::Node& node : graph.nodes()) {
            const wng::NodeDefinition* node_definition = schema.find_node_definition(node.type);
            if (node_definition == nullptr) {
                add_issue(report, wng::ValidationIssueCode::MissingNodeDefinition, wng::Result::NotFound, node.id, wng::PortId {}, wng::LinkId {}, "schema does not define this graph node type");
                continue;
            }

            if (!node_definition->enabled) {
                add_issue(report, wng::ValidationIssueCode::DisabledNodeDefinition, wng::Result::InvalidConnection, node.id, wng::PortId {}, wng::LinkId {}, "schema node definition is disabled");
            }

            validate_schema_ports_for_node(graph, node, *node_definition, report);
        }

        for (const wng::Node& node : graph.nodes()) {
            const wng::NodeDefinition* node_definition = schema.find_node_definition(node.type);
            if (node_definition == nullptr) {
                continue;
            }

            validate_required_ports(graph, node, node_definition->inputs, report);
            validate_required_ports(graph, node, node_definition->outputs, report);
        }
    }

    wng::Result normalized_connection_callback_result(const wng::ConnectionValidation& validation)
    {
        if (validation.result != wng::Result::Ok) {
            return validation.result;
        }

        return validation.status == wng::ConnectionStatus::Allowed ?
            wng::Result::Ok :
            wng::Result::InvalidConnection;
    }

    void append_schema_connection_validation(
        const wng::Graph& graph,
        const wng::GraphSchema& schema,
        const wng::SchemaValidationOptions& options,
        wng::ValidationReport& report)
    {
        if (options.callback == nullptr || report.has_errors()) {
            return;
        }

        // Existing links have already passed structural and built-in schema checks.
        // Calling the proposed-connection validator here would reject the same
        // links as duplicates, so only the shared host schema callback is applied.
        for (const wng::Link& link : graph.links()) {
            try {
                const wng::ConnectionValidation validation =
                    options.callback->validate_connection(graph, schema, link.from, link.to);
                const wng::Result result = normalized_connection_callback_result(validation);
                if (validation.status == wng::ConnectionStatus::Allowed && result == wng::Result::Ok) {
                    continue;
                }

                add_issue(
                    report,
                    wng::ValidationIssueCode::SchemaConnectionRejected,
                    result,
                    wng::NodeId {},
                    wng::PortId {},
                    link.id,
                    "schema connection callback rejected link");
            } catch (const std::bad_alloc&) {
                add_issue(
                    report,
                    wng::ValidationIssueCode::SchemaConnectionRejected,
                    wng::Result::AllocationFailure,
                    wng::NodeId {},
                    wng::PortId {},
                    link.id,
                    "allocation failed while running schema connection callback");
                return;
            }
        }
    }

    wng::ValidationReport validate_graph_structural(
        const wng::Graph& graph,
        const wng::GraphValidationOptions& options)
    {
        wng::ValidationReport report;

        // Deterministic issue order is part of the API contract for tests and
        // future editor diagnostics. Keep these passes ordered by graph storage.
        validate_node_list(graph, report);
        validate_port_list(graph, report);
        validate_node_owned_port_references(graph, report);
        validate_links(graph, report);
        validate_acyclic_mode(graph, options, report);

        return report;
    }
}

namespace wng
{
    bool ValidationReport::valid() const
    {
        return !has_errors();
    }

    bool ValidationReport::has_errors() const
    {
        for (const ValidationIssue& issue : issues) {
            if (issue.severity == ValidationSeverity::Error) {
                return true;
            }
        }

        return false;
    }

    ValidationReport validate_graph(const Graph& graph)
    {
        return validate_graph(graph, GraphValidationOptions {});
    }

    ValidationReport validate_graph(
        const Graph& graph,
        const GraphValidationOptions& options)
    {
        try {
            ValidationReport report = validate_graph_structural(graph, options);
            append_host_validation(graph, options, report);
            return report;
        } catch (const std::bad_alloc&) {
            return allocation_failure_report();
        }
    }

    ValidationReport validate_graph(const Graph& graph, const GraphSchema& schema)
    {
        return validate_graph(graph, schema, GraphSchemaValidationOptions {});
    }

    ValidationReport validate_graph(
        const Graph& graph,
        const GraphSchema& schema,
        const GraphValidationOptions& options)
    {
        GraphSchemaValidationOptions combined_options;
        combined_options.graph_options = options;
        return validate_graph(graph, schema, combined_options);
    }

    ValidationReport validate_graph(
        const Graph& graph,
        const GraphSchema& schema,
        const GraphSchemaValidationOptions& options)
    {
        try {
            ValidationReport report = validate_graph_structural(graph, options.graph_options);
            validate_against_schema(graph, schema, report);
            append_schema_connection_validation(graph, schema, options.schema_options, report);
            append_host_validation(graph, options.graph_options, report);
            return report;
        } catch (const std::bad_alloc&) {
            return allocation_failure_report();
        }
    }
}
