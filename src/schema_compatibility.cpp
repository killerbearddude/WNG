// Implements read-only schema compatibility analysis for WNG graphs.
// The analyzer composes existing schema validation and schema diffing; it does
// not mutate graphs, repair schemas, apply patches, or perform migration.

#include <new>
#include <string>
#include <vector>

#include <wng/schema_compatibility.hpp>

#include <wng/graph.hpp>
#include <wng/schema.hpp>

namespace
{
    wng::SchemaCompatibilityReport compatibility_failure(wng::Result result)
    {
        wng::SchemaCompatibilityReport report;
        report.result = result;
        return report;
    }

    wng::SchemaCompatibilityStatus compute_status(bool source_valid, bool target_valid)
    {
        if (source_valid && target_valid) {
            return wng::SchemaCompatibilityStatus::Compatible;
        }

        if (!source_valid && target_valid) {
            return wng::SchemaCompatibilityStatus::SourceInvalid;
        }

        if (source_valid && !target_valid) {
            return wng::SchemaCompatibilityStatus::TargetInvalid;
        }

        return wng::SchemaCompatibilityStatus::SourceAndTargetInvalid;
    }

    bool contains_node_id(const std::vector<wng::NodeId>& ids, wng::NodeId id)
    {
        for (wng::NodeId existing : ids) {
            if (existing == id) {
                return true;
            }
        }

        return false;
    }

    bool contains_port_id(const std::vector<wng::PortId>& ids, wng::PortId id)
    {
        for (wng::PortId existing : ids) {
            if (existing == id) {
                return true;
            }
        }

        return false;
    }

    void append_unique_node_id(std::vector<wng::NodeId>& ids, wng::NodeId id)
    {
        if (!contains_node_id(ids, id)) {
            ids.push_back(id);
        }
    }

    void append_unique_port_id(std::vector<wng::PortId>& ids, wng::PortId id)
    {
        if (!contains_port_id(ids, id)) {
            ids.push_back(id);
        }
    }

    bool node_type_is_changed(const wng::SchemaDiff& diff, const std::string& type)
    {
        for (const wng::NodeDefinitionDiff& node_diff : diff.nodes) {
            if (node_diff.type == type) {
                return true;
            }
        }

        return false;
    }

    bool node_type_is_removed(const wng::SchemaDiff& diff, const std::string& type)
    {
        for (const wng::NodeDefinitionDiff& node_diff : diff.nodes) {
            if (node_diff.type == type && node_diff.change == wng::SchemaDiffChange::Removed) {
                return true;
            }
        }

        return false;
    }

    bool node_type_has_new_required_port(const wng::SchemaDiff& diff, const std::string& type)
    {
        for (const wng::PortDefinitionDiff& port_diff : diff.ports) {
            if (port_diff.node_type == type &&
                port_diff.change != wng::SchemaDiffChange::Removed &&
                port_diff.after.required) {
                return true;
            }
        }

        return false;
    }

    bool port_definition_affects_port(
        const wng::PortDefinitionDiff& diff,
        const wng::Node& node,
        const wng::Port& port)
    {
        return node.type == diff.node_type &&
               port.kind == diff.kind &&
               port.name == diff.name;
    }

    void append_affected_nodes(
        const wng::Graph& graph,
        const wng::SchemaDiff& diff,
        std::vector<wng::NodeId>& out_nodes)
    {
        // Affected node order follows graph storage order. The schema diff only
        // decides whether a stored node is relevant; it does not reorder graph IDs.
        for (const wng::Node& node : graph.nodes()) {
            if (node_type_is_changed(diff, node.type) ||
                node_type_has_new_required_port(diff, node.type)) {
                append_unique_node_id(out_nodes, node.id);
            }
        }
    }

    void append_affected_ports(
        const wng::Graph& graph,
        const wng::SchemaDiff& diff,
        std::vector<wng::PortId>& out_ports)
    {
        // Removed node definitions make every owned port diagnostically relevant.
        // Otherwise, port diffs target the current schema port identity rule:
        // owner node type, port kind, and port name.
        for (const wng::Port& port : graph.ports()) {
            const wng::Node* node = graph.find_node(port.node);
            if (node == nullptr) {
                continue;
            }

            if (node_type_is_removed(diff, node->type)) {
                append_unique_port_id(out_ports, port.id);
                continue;
            }

            for (const wng::PortDefinitionDiff& port_diff : diff.ports) {
                if (port_definition_affects_port(port_diff, *node, port)) {
                    append_unique_port_id(out_ports, port.id);
                    break;
                }
            }
        }
    }
}

namespace wng
{
    bool SchemaCompatibilityReport::compatible() const
    {
        return result == Result::Ok && status == SchemaCompatibilityStatus::Compatible;
    }

    bool SchemaCompatibilityReport::success() const
    {
        return result == Result::Ok;
    }

    SchemaCompatibilityReport analyze_schema_compatibility(
        const Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema)
    {
        try {
            SchemaCompatibilityReport report;

            // Result reports whether analysis itself completed. Validation
            // failures are diagnostic outcomes and are represented by status and
            // the retained validation reports instead of a non-Ok analysis result.
            report.schema_diff = diff_schemas(source_schema, target_schema);
            if (!report.schema_diff.success()) {
                report.result = report.schema_diff.result;
                return report;
            }

            report.source_validation = validate_graph(graph, source_schema);
            report.target_validation = validate_graph(graph, target_schema);
            report.status = compute_status(
                report.source_validation.valid(),
                report.target_validation.valid());

            // Affected IDs are derived from schema changes and current graph
            // storage. They are best-effort diagnostics for future migration
            // planning; no migration or graph mutation happens here.
            append_affected_nodes(graph, report.schema_diff, report.affected_nodes);
            append_affected_ports(graph, report.schema_diff, report.affected_ports);

            report.result = Result::Ok;
            return report;
        } catch (const std::bad_alloc&) {
            return compatibility_failure(Result::AllocationFailure);
        }
    }
}
