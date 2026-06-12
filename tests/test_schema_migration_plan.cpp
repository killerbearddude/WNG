// Exercises read-only schema migration planning for graph/schema transitions.
// Tests verify deterministic action derivation, blocking diagnostics, embedded
// compatibility reports, and the no-mutation guarantee.

#include <cassert>
#include <string>
#include <vector>

#include <wng/graph.hpp>
#include <wng/schema_migration_plan.hpp>
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
        definition.display_name = type;
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

    wng::GraphSchema make_schema(
        const wng::NodeDefinition& first,
        const wng::NodeDefinition& second)
    {
        wng::GraphSchema schema;
        assert(schema.add_node_definition(first) == wng::Result::Ok);
        assert(schema.add_node_definition(second) == wng::Result::Ok);
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

    const wng::SchemaMigrationAction* find_action(
        const wng::SchemaMigrationPlan& plan,
        wng::SchemaMigrationActionKind kind,
        const std::string& node_type,
        const std::string& port_name = "")
    {
        for (const wng::SchemaMigrationAction& action : plan.actions) {
            if (action.kind == kind &&
                action.node_type == node_type &&
                (port_name.empty() || action.port_name == port_name)) {
                return &action;
            }
        }

        return nullptr;
    }
}

int main()
{
    {
        // Identical schemas are the no-op baseline. Planning should succeed,
        // report compatibility, and emit no migration actions.
        const wng::GraphSchema schema = make_schema(make_node_definition());
        wng::Graph graph;
        instantiate_schema_node(graph, schema);

        const wng::SchemaMigrationPlan plan =
            wng::build_schema_migration_plan(graph, schema, schema);

        assert(plan.result == wng::Result::Ok);
        assert(plan.success());
        assert(plan.compatible());
        assert(!plan.blocked());
        assert(plan.empty());
        assert(plan.actions.empty());
    }

    {
        // Removing a used node type becomes a blocking action without deleting
        // anything. Future migration application must remain a separate layer.
        const wng::GraphSchema source = make_schema(make_node_definition());
        const wng::GraphSchema target;
        wng::Graph graph;
        const wng::NodeId node = instantiate_schema_node(graph, source);
        const std::size_t node_count = graph.nodes().size();
        const std::size_t port_count = graph.ports().size();

        const wng::SchemaMigrationPlan plan =
            wng::build_schema_migration_plan(graph, source, target);
        const wng::SchemaMigrationAction* action = find_action(
            plan,
            wng::SchemaMigrationActionKind::RemoveNodeType,
            "math.add");

        assert(action != nullptr);
        assert(action->blocking);
        assert(contains_node_id(action->affected_nodes, node));
        assert(action->affected_ports.size() == port_count);
        assert(plan.blocked());
        assert(graph.nodes().size() == node_count);
        assert(graph.ports().size() == port_count);
    }

    {
        // Removing an unused node type is still useful migration information, but
        // it must not block the graph because no current graph nodes depend on it.
        const wng::GraphSchema source = make_schema(make_node_definition("math.unused"));
        const wng::GraphSchema target;
        const wng::Graph graph;

        const wng::SchemaMigrationPlan plan =
            wng::build_schema_migration_plan(graph, source, target);
        const wng::SchemaMigrationAction* action = find_action(
            plan,
            wng::SchemaMigrationActionKind::RemoveNodeType,
            "math.unused");

        assert(action != nullptr);
        assert(action->affected_nodes.empty());
        assert(!action->blocking);
        assert(!plan.blocked());
    }

    {
        // Removing a port definition that exists on graph nodes is blocking. The
        // plan reports both the matching ports and their owning nodes.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.clear();
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId node = instantiate_schema_node(graph, source);
        const wng::Port* value = find_port(graph, node, wng::PortKind::Input, "value");
        assert(value != nullptr);

        const wng::SchemaMigrationPlan plan =
            wng::build_schema_migration_plan(graph, source, target);
        const wng::SchemaMigrationAction* action = find_action(
            plan,
            wng::SchemaMigrationActionKind::RemovePortDefinition,
            "math.add",
            "value");

        assert(action != nullptr);
        assert(action->blocking);
        assert(contains_port_id(action->affected_ports, value->id));
        assert(contains_node_id(action->affected_nodes, node));
        assert(plan.blocked());
    }

    {
        // Port identity is stable across type changes, so the plan emits a
        // ModifyPortDefinition action rather than remove/add actions.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs[0].type = "string";
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId node = instantiate_schema_node(graph, source);
        const wng::Port* value = find_port(graph, node, wng::PortKind::Input, "value");
        assert(value != nullptr);

        const wng::SchemaMigrationPlan plan =
            wng::build_schema_migration_plan(graph, source, target);
        const wng::SchemaMigrationAction* action = find_action(
            plan,
            wng::SchemaMigrationActionKind::ModifyPortDefinition,
            "math.add",
            "value");

        assert(action != nullptr);
        assert(contains_port_id(action->affected_ports, value->id));
        assert(contains_node_id(action->affected_nodes, node));
        assert(action->blocking);
        assert(plan.blocked());
    }

    {
        // Added optional ports do not require existing graph objects to change.
        // The planner intentionally omits optional additions unless they affect
        // compatibility.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.push_back(input("optional", "number", false));
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source);

        const wng::SchemaMigrationPlan plan =
            wng::build_schema_migration_plan(graph, source, target);

        assert(plan.result == wng::Result::Ok);
        assert(!plan.blocked());
        assert(find_action(
            plan,
            wng::SchemaMigrationActionKind::AddRequiredPort,
            "math.add",
            "optional") == nullptr);
    }

    {
        // Added required ports create blocking actions for graph nodes missing
        // the new required port. The planner reports intent only; it does not
        // create the missing port.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.push_back(input("extra", "number", true));
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId node = instantiate_schema_node(graph, source);

        const wng::SchemaMigrationPlan plan =
            wng::build_schema_migration_plan(graph, source, target);
        const wng::SchemaMigrationAction* action = find_action(
            plan,
            wng::SchemaMigrationActionKind::AddRequiredPort,
            "math.add",
            "extra");

        assert(action != nullptr);
        assert(action->blocking);
        assert(contains_node_id(action->affected_nodes, node));
        assert(action->affected_ports.empty());
        assert(find_port(graph, node, wng::PortKind::Input, "extra") == nullptr);
    }

    {
        // A target-invalid graph with no schema diff still produces a blocking
        // fallback action, preventing migration plans from silently passing over
        // uncategorized target validation issues.
        const wng::GraphSchema source;
        const wng::GraphSchema target;
        wng::Graph graph;
        create_bare_node(graph, "missing.node");

        const wng::SchemaMigrationPlan plan =
            wng::build_schema_migration_plan(graph, source, target);

        assert(plan.compatibility.status == wng::SchemaCompatibilityStatus::SourceAndTargetInvalid);
        assert(plan.blocked());
        assert(!plan.actions.empty());
        assert(plan.actions.back().kind == wng::SchemaMigrationActionKind::TargetValidationIssue);
        assert(plan.actions.back().blocking);
    }

    {
        // If the source rejects the graph but the target accepts it, the planner
        // keeps the compatibility status but does not invent a target migration
        // blocker.
        const wng::GraphSchema source;
        const wng::GraphSchema target = make_schema(make_node_definition());
        wng::Graph graph;
        instantiate_schema_node(graph, target);

        const wng::SchemaMigrationPlan plan =
            wng::build_schema_migration_plan(graph, source, target);

        assert(plan.result == wng::Result::Ok);
        assert(plan.compatibility.status == wng::SchemaCompatibilityStatus::SourceInvalid);
        assert(!plan.blocked());
    }

    {
        // Action category order is part of the public behavior. Tests and future
        // migration UI can rely on broad categories appearing in deterministic
        // planning order.
        wng::NodeDefinition removed_definition;
        removed_definition.type = "removed.type";
        removed_definition.display_name = "Removed";
        const wng::GraphSchema source = make_schema(
            removed_definition,
            make_node_definition("math.add"));
        wng::NodeDefinition target_math = make_node_definition("math.add", false);
        target_math.inputs[0].type = "string";
        target_math.inputs.push_back(input("extra", "number", true));
        target_math.outputs.clear();
        const wng::GraphSchema target = make_schema(target_math);
        wng::Graph graph;
        instantiate_schema_node(graph, source, "math.add");

        const wng::SchemaMigrationPlan plan =
            wng::build_schema_migration_plan(graph, source, target);

        assert(plan.actions.size() >= 5);
        assert(plan.actions[0].kind == wng::SchemaMigrationActionKind::RemoveNodeType);
        assert(plan.actions[1].kind == wng::SchemaMigrationActionKind::ModifyNodeType);
        assert(plan.actions[2].kind == wng::SchemaMigrationActionKind::RemovePortDefinition);
        assert(plan.actions[3].kind == wng::SchemaMigrationActionKind::ModifyPortDefinition);
        assert(plan.actions[4].kind == wng::SchemaMigrationActionKind::AddRequiredPort);
    }

    {
        // Affected IDs preserve graph storage order. Stable ordering keeps
        // migration diagnostics reproducible across runs.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs[0].type = "string";
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId first = instantiate_schema_node(graph, source);
        const wng::NodeId second = instantiate_schema_node(graph, source);

        const wng::SchemaMigrationPlan plan =
            wng::build_schema_migration_plan(graph, source, target);
        const wng::SchemaMigrationAction* action = find_action(
            plan,
            wng::SchemaMigrationActionKind::ModifyPortDefinition,
            "math.add",
            "value");

        assert(action != nullptr);
        assert(action->affected_nodes.size() == 2);
        assert(action->affected_nodes[0] == first);
        assert(action->affected_nodes[1] == second);
        assert(action->affected_ports.size() == 2);
    }

    {
        // Planning is read-only. This protects the boundary between diagnostics
        // and future explicit migration application.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs[0].type = "string";
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source);
        const std::size_t node_count = graph.nodes().size();
        const std::size_t port_count = graph.ports().size();
        const std::size_t source_definition_count = source.node_definitions().size();
        const std::size_t target_definition_count = target.node_definitions().size();

        const wng::SchemaMigrationPlan plan =
            wng::build_schema_migration_plan(graph, source, target);

        assert(plan.result == wng::Result::Ok);
        assert(graph.nodes().size() == node_count);
        assert(graph.ports().size() == port_count);
        assert(source.node_definitions().size() == source_definition_count);
        assert(target.node_definitions().size() == target_definition_count);
    }

    {
        // The plan intentionally embeds compatibility details instead of reducing
        // them to actions. Future callers can inspect target validation and schema
        // diffs without recomputing analysis.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs[0].type = "string";
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source);

        const wng::SchemaMigrationPlan plan =
            wng::build_schema_migration_plan(graph, source, target);

        assert(plan.compatibility.schema_diff.changed());
        assert(!plan.compatibility.target_validation.valid());
    }

    return 0;
}
