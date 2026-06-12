// Exercises read-only compatibility reports for graph/schema transitions.
// Tests focus on diagnostic composition: schema diffing, source/target
// validation, affected IDs, and the no-mutation guarantee.

#include <cassert>
#include <string>
#include <vector>

#include <wng/execution_plan.hpp>
#include <wng/graph.hpp>
#include <wng/schema_compatibility.hpp>
#include <wng/schema_mutation.hpp>

namespace
{
    wng::PortDefinition input(
        const std::string& name,
        const std::string& type = "number",
        bool required = true)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Input;
        definition.type = type;
        definition.required = required;
        return definition;
    }

    wng::PortDefinition output(
        const std::string& name,
        const std::string& type = "number",
        bool required = false)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Output;
        definition.type = type;
        definition.required = required;
        return definition;
    }

    wng::NodeDefinition make_node_definition(
        const std::string& type = "math.add",
        bool enabled = true)
    {
        wng::NodeDefinition definition;
        definition.type = type;
        definition.display_name = "Add";
        definition.inputs.push_back(input("value", "number", true));
        definition.outputs.push_back(output("result", "number", false));
        definition.enabled = enabled;
        return definition;
    }

    wng::GraphSchema make_schema(const wng::NodeDefinition& definition)
    {
        wng::GraphSchema schema;
        assert(schema.add_node_definition(definition) == wng::Result::Ok);
        return schema;
    }

    wng::NodeDesc make_node_desc(const std::string& type = "math.add")
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = type;
        desc.size = { 100.0f, 60.0f };
        return desc;
    }

    wng::NodeId instantiate_schema_node(
        wng::Graph& graph,
        const wng::GraphSchema& schema,
        const std::string& type = "math.add")
    {
        wng::NodeId node;
        assert(wng::instantiate_node(graph, schema, make_node_desc(type), &node, nullptr) ==
            wng::Result::Ok);
        return node;
    }

    wng::NodeId create_bare_node(wng::Graph& graph, const std::string& type)
    {
        wng::NodeId node;
        assert(graph.create_node(make_node_desc(type), &node) == wng::Result::Ok);
        return node;
    }

    const wng::Port* find_port(
        const wng::Graph& graph,
        wng::NodeId node,
        wng::PortKind kind,
        const std::string& name)
    {
        for (const wng::Port& port : graph.ports()) {
            if (port.node == node && port.kind == kind && port.name == name) {
                return &port;
            }
        }

        return nullptr;
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

    bool has_node_diff(
        const wng::SchemaCompatibilityReport& report,
        wng::SchemaDiffChange change,
        const std::string& type)
    {
        for (const wng::NodeDefinitionDiff& diff : report.schema_diff.nodes) {
            if (diff.change == change && diff.type == type) {
                return true;
            }
        }

        return false;
    }

    bool has_port_diff(
        const wng::SchemaCompatibilityReport& report,
        wng::SchemaDiffChange change,
        const std::string& node_type,
        wng::PortKind kind,
        const std::string& name)
    {
        for (const wng::PortDefinitionDiff& diff : report.schema_diff.ports) {
            if (diff.change == change &&
                diff.node_type == node_type &&
                diff.kind == kind &&
                diff.name == name) {
                return true;
            }
        }

        return false;
    }
}

int main()
{
    {
        // Empty schemas and an empty graph are the compatibility baseline. This
        // protects the status/result split: compatible analysis still returns Ok.
        const wng::Graph graph;
        const wng::GraphSchema source;
        const wng::GraphSchema target;

        const wng::SchemaCompatibilityReport report =
            wng::analyze_schema_compatibility(graph, source, target);

        assert(report.result == wng::Result::Ok);
        assert(report.status == wng::SchemaCompatibilityStatus::Compatible);
        assert(report.compatible());
        assert(report.success());
        assert(report.schema_diff.empty());
        assert(report.source_validation.valid());
        assert(report.target_validation.valid());
        assert(report.affected_nodes.empty());
        assert(report.affected_ports.empty());
    }

    {
        // Identical source and target schemas should not flag existing graph IDs
        // as affected. Future migration planning depends on stable no-op reports.
        const wng::GraphSchema schema = make_schema(make_node_definition());
        wng::Graph graph;
        instantiate_schema_node(graph, schema);

        const wng::SchemaCompatibilityReport report =
            wng::analyze_schema_compatibility(graph, schema, schema);

        assert(report.result == wng::Result::Ok);
        assert(report.status == wng::SchemaCompatibilityStatus::Compatible);
        assert(report.compatible());
        assert(report.schema_diff.empty());
        assert(report.affected_nodes.empty());
        assert(report.affected_ports.empty());
    }

    {
        // Removing a node definition is a target-schema compatibility break. The
        // analyzer reports graph nodes and owned ports of that removed type but
        // does not mutate the graph.
        const wng::GraphSchema source = make_schema(make_node_definition());
        const wng::GraphSchema target;
        wng::Graph graph;
        const wng::NodeId node = instantiate_schema_node(graph, source);
        const std::size_t node_count = graph.nodes().size();
        const std::size_t port_count = graph.ports().size();

        const wng::SchemaCompatibilityReport report =
            wng::analyze_schema_compatibility(graph, source, target);

        assert(report.result == wng::Result::Ok);
        assert(report.status == wng::SchemaCompatibilityStatus::TargetInvalid);
        assert(!report.compatible());
        assert(report.source_validation.valid());
        assert(!report.target_validation.valid());
        assert(has_node_diff(report, wng::SchemaDiffChange::Removed, "math.add"));
        assert(contains_node_id(report.affected_nodes, node));
        assert(report.affected_ports.size() == graph.ports().size());
        assert(graph.nodes().size() == node_count);
        assert(graph.ports().size() == port_count);
    }

    {
        // A graph that the source schema rejects but the target schema accepts is
        // reported separately from a migration break. This identifies already-bad
        // source states without hiding that the target would accept them.
        const wng::GraphSchema source;
        const wng::GraphSchema target = make_schema(make_node_definition());
        wng::Graph graph;
        instantiate_schema_node(graph, target);

        const wng::SchemaCompatibilityReport report =
            wng::analyze_schema_compatibility(graph, source, target);

        assert(report.result == wng::Result::Ok);
        assert(report.status == wng::SchemaCompatibilityStatus::SourceInvalid);
        assert(!report.source_validation.valid());
        assert(report.target_validation.valid());
    }

    {
        // Missing node definitions in both schemas report SourceAndTargetInvalid.
        // This is diagnostic-only; there is no attempted repair or migration.
        const wng::GraphSchema source;
        const wng::GraphSchema target;
        wng::Graph graph;
        create_bare_node(graph, "missing.node");

        const wng::SchemaCompatibilityReport report =
            wng::analyze_schema_compatibility(graph, source, target);

        assert(report.result == wng::Result::Ok);
        assert(report.status == wng::SchemaCompatibilityStatus::SourceAndTargetInvalid);
        assert(!report.source_validation.valid());
        assert(!report.target_validation.valid());
    }

    {
        // A port type change keeps the same schema identity and is reported as a
        // modified port, not as remove/add. Matching graph ports are marked
        // affected in graph storage order.
        wng::NodeDefinition source_definition = make_node_definition();
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs[0].type = "string";
        const wng::GraphSchema source = make_schema(source_definition);
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId node = instantiate_schema_node(graph, source);
        const wng::Port* value = find_port(graph, node, wng::PortKind::Input, "value");
        assert(value != nullptr);

        const wng::SchemaCompatibilityReport report =
            wng::analyze_schema_compatibility(graph, source, target);

        assert(report.result == wng::Result::Ok);
        assert(report.status == wng::SchemaCompatibilityStatus::TargetInvalid);
        assert(has_port_diff(report, wng::SchemaDiffChange::Modified, "math.add", wng::PortKind::Input, "value"));
        assert(contains_port_id(report.affected_ports, value->id));
    }

    {
        // Adding an optional schema port is reported in the schema diff but does
        // not affect existing graph objects or target validation.
        wng::NodeDefinition source_definition = make_node_definition();
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.push_back(input("optional", "number", false));
        const wng::GraphSchema source = make_schema(source_definition);
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source);

        const wng::SchemaCompatibilityReport report =
            wng::analyze_schema_compatibility(graph, source, target);

        assert(report.result == wng::Result::Ok);
        assert(report.status == wng::SchemaCompatibilityStatus::Compatible);
        assert(has_port_diff(report, wng::SchemaDiffChange::Added, "math.add", wng::PortKind::Input, "optional"));
        assert(report.affected_nodes.empty());
        assert(report.affected_ports.empty());
    }

    {
        // Adding a required schema port is a target compatibility break for
        // existing nodes of that type because the current graph lacks the new
        // required port.
        wng::NodeDefinition source_definition = make_node_definition();
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.push_back(input("required_extra", "number", true));
        const wng::GraphSchema source = make_schema(source_definition);
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId node = instantiate_schema_node(graph, source);

        const wng::SchemaCompatibilityReport report =
            wng::analyze_schema_compatibility(graph, source, target);

        assert(report.result == wng::Result::Ok);
        assert(report.status == wng::SchemaCompatibilityStatus::TargetInvalid);
        assert(has_port_diff(report, wng::SchemaDiffChange::Added, "math.add", wng::PortKind::Input, "required_extra"));
        assert(contains_node_id(report.affected_nodes, node));
    }

    {
        // Disabling a node definition changes scalar schema state and affects all
        // graph nodes of that type. Validation failure is represented by status,
        // not by a failed analysis result.
        const wng::GraphSchema source = make_schema(make_node_definition("math.add", true));
        const wng::GraphSchema target = make_schema(make_node_definition("math.add", false));
        wng::Graph graph;
        const wng::NodeId node = instantiate_schema_node(graph, source);

        const wng::SchemaCompatibilityReport report =
            wng::analyze_schema_compatibility(graph, source, target);

        assert(report.result == wng::Result::Ok);
        assert(report.status == wng::SchemaCompatibilityStatus::TargetInvalid);
        assert(has_node_diff(report, wng::SchemaDiffChange::Modified, "math.add"));
        assert(contains_node_id(report.affected_nodes, node));
    }

    {
        // Affected IDs must follow graph storage order so future diagnostics and
        // editor presentations remain reproducible.
        const wng::GraphSchema source = make_schema(make_node_definition("math.add", true));
        const wng::GraphSchema target = make_schema(make_node_definition("math.add", false));
        wng::Graph graph;
        const wng::NodeId first = instantiate_schema_node(graph, source);
        const wng::NodeId second = instantiate_schema_node(graph, source);

        const wng::SchemaCompatibilityReport report =
            wng::analyze_schema_compatibility(graph, source, target);

        assert(report.result == wng::Result::Ok);
        assert(report.affected_nodes.size() == 2);
        assert(report.affected_nodes[0] == first);
        assert(report.affected_nodes[1] == second);
    }

    {
        // Compatibility analysis is read-only. Calling it must not change graph
        // object counts or schema definition counts because migration is a future
        // layer, not part of this diagnostic helper.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.push_back(input("required_extra", "number", true));
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source);
        const std::size_t graph_nodes = graph.nodes().size();
        const std::size_t graph_ports = graph.ports().size();
        const std::size_t source_definitions = source.node_definitions().size();
        const std::size_t target_definitions = target.node_definitions().size();

        const wng::SchemaCompatibilityReport report =
            wng::analyze_schema_compatibility(graph, source, target);

        assert(report.result == wng::Result::Ok);
        assert(graph.nodes().size() == graph_nodes);
        assert(graph.ports().size() == graph_ports);
        assert(source.node_definitions().size() == source_definitions);
        assert(target.node_definitions().size() == target_definitions);
    }

    return 0;
}
