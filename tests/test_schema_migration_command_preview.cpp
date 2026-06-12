// Exercises read-only schema migration command previews.
// Tests verify prospective operation derivation, deterministic ordering,
// blocked-status mapping, and the no-mutation/no-command-record boundary.

#include <cassert>
#include <string>
#include <vector>

#include <wng/graph.hpp>
#include <wng/schema_migration_command_preview.hpp>
#include <wng/schema_mutation.hpp>

namespace
{
    wng::PortDefinition input(
        const std::string& name,
        const std::string& type = "number",
        bool required = false)
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
        bool with_default_ports = true)
    {
        wng::NodeDefinition definition;
        definition.type = type;
        definition.display_name = type;
        if (with_default_ports) {
            definition.inputs.push_back(input("value", "number", false));
            definition.outputs.push_back(output("result", "number", false));
        }
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

    wng::GraphSchema make_schema(
        const wng::NodeDefinition& first,
        const wng::NodeDefinition& second,
        const wng::NodeDefinition& third)
    {
        wng::GraphSchema schema;
        assert(schema.add_node_definition(first) == wng::Result::Ok);
        assert(schema.add_node_definition(second) == wng::Result::Ok);
        assert(schema.add_node_definition(third) == wng::Result::Ok);
        return schema;
    }

    wng::NodeDesc make_node_desc(const std::string& type)
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
        const std::string& type)
    {
        wng::NodeId node;
        assert(wng::instantiate_node(graph, schema, make_node_desc(type), &node, nullptr) ==
            wng::Result::Ok);
        return node;
    }

    wng::PortDefinitionIdentity port_identity(
        const std::string& node_type,
        wng::PortKind kind,
        const std::string& name)
    {
        wng::PortDefinitionIdentity identity;
        identity.node_type = node_type;
        identity.kind = kind;
        identity.name = name;
        return identity;
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

    const wng::SchemaMigrationCommandPreviewStep* find_step(
        const wng::SchemaMigrationCommandPreview& preview,
        wng::SchemaMigrationCommandPreviewStepKind kind)
    {
        for (const wng::SchemaMigrationCommandPreviewStep& step : preview.steps) {
            if (step.kind == kind) {
                return &step;
            }
        }

        return nullptr;
    }
}

int main()
{
    {
        // Identical schemas need no prospective operation. Command preview stays
        // ready and empty instead of fabricating no-op command records.
        const wng::GraphSchema schema = make_schema(make_node_definition());
        wng::Graph graph;
        instantiate_schema_node(graph, schema, "math.add");
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationCommandPreview preview =
            wng::preview_schema_migration_commands(graph, schema, schema, policy);

        assert(preview.result == wng::Result::Ok);
        assert(preview.status == wng::SchemaMigrationCommandPreviewStatus::Ready);
        assert(preview.ready());
        assert(!preview.blocked());
        assert(preview.empty());
    }

    {
        // Structurally invalid policy is rejected by the embedded apply preview,
        // so speculative command generation never sees ambiguous mappings.
        const wng::GraphSchema schema = make_schema(make_node_definition());
        const wng::Graph graph;
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "math.add", "math.add" });

        const wng::SchemaMigrationCommandPreview preview =
            wng::preview_schema_migration_commands(graph, schema, schema, policy);

        assert(preview.result == wng::Result::InvalidArgument);
        assert(preview.status == wng::SchemaMigrationCommandPreviewStatus::PolicyInvalid);
        assert(!preview.apply_preview.policy_validation.valid());
        assert(preview.steps.empty());
    }

    {
        // Uncovered blocking actions stop command preview. A future apply layer
        // must not guess how to remove graph data without explicit policy.
        const wng::GraphSchema source = make_schema(make_node_definition());
        const wng::GraphSchema target;
        wng::Graph graph;
        instantiate_schema_node(graph, source, "math.add");
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationCommandPreview preview =
            wng::preview_schema_migration_commands(graph, source, target, policy);

        assert(preview.result == wng::Result::Ok);
        assert(preview.status ==
            wng::SchemaMigrationCommandPreviewStatus::BlockedByUncoveredActions);
        assert(preview.blocked());
        assert(preview.steps.empty());
    }

    {
        // Node-type rename remains speculative. The step reports affected graph
        // nodes, but the node type stored in Graph is not changed.
        const wng::GraphSchema source = make_schema(make_node_definition("old.math.add", false));
        const wng::GraphSchema target = make_schema(make_node_definition("math.add", false));
        wng::Graph graph;
        const wng::NodeId node = instantiate_schema_node(graph, source, "old.math.add");
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.math.add", "math.add" });

        const wng::SchemaMigrationCommandPreview preview =
            wng::preview_schema_migration_commands(graph, source, target, policy);
        const wng::SchemaMigrationCommandPreviewStep* step = find_step(
            preview,
            wng::SchemaMigrationCommandPreviewStepKind::RenameNodeType);

        assert(preview.status == wng::SchemaMigrationCommandPreviewStatus::Ready);
        assert(step != nullptr);
        assert(step->from_node_type == "old.math.add");
        assert(step->to_node_type == "math.add");
        assert(contains_node_id(step->affected_nodes, node));
        assert(!step->destructive);
        assert(step->policy_covered);
        assert(graph.find_node(node)->type == "old.math.add");
    }

    {
        // Port rename preview describes matching graph ports without creating the
        // target port identity or editing the existing port name.
        wng::NodeDefinition source_definition = make_node_definition();
        source_definition.inputs[0].name = "lhs";
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs[0].name = "a";
        target_definition.inputs[0].required = false;
        const wng::GraphSchema source = make_schema(source_definition);
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source, "math.add");
        wng::SchemaMigrationPolicy policy;
        wng::PortDefinitionRenamePolicy rename;
        rename.from = port_identity("math.add", wng::PortKind::Input, "lhs");
        rename.to = port_identity("math.add", wng::PortKind::Input, "a");
        policy.port_renames.push_back(rename);

        const wng::SchemaMigrationCommandPreview preview =
            wng::preview_schema_migration_commands(graph, source, target, policy);
        const wng::SchemaMigrationCommandPreviewStep* step = find_step(
            preview,
            wng::SchemaMigrationCommandPreviewStepKind::RenamePortDefinition);

        assert(preview.status == wng::SchemaMigrationCommandPreviewStatus::Ready);
        assert(step != nullptr);
        assert(step->from_port.name == "lhs");
        assert(step->to_port.name == "a");
        assert(step->affected_ports.size() == 1);
        assert(!step->destructive);
        assert(step->policy_covered);
        assert(graph.ports()[0].name == "lhs");
    }

    {
        // Port type-change preview reports prospective coercion targets. The port
        // type in Graph remains unchanged until a future apply layer exists.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs[0].type = "scalar";
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source, "math.add");
        wng::SchemaMigrationPolicy policy;
        wng::PortTypeChangePolicy change;
        change.port = port_identity("math.add", wng::PortKind::Input, "value");
        change.from_type = "number";
        change.to_type = "scalar";
        policy.port_type_changes.push_back(change);

        const wng::SchemaMigrationCommandPreview preview =
            wng::preview_schema_migration_commands(graph, source, target, policy);
        const wng::SchemaMigrationCommandPreviewStep* step = find_step(
            preview,
            wng::SchemaMigrationCommandPreviewStepKind::ChangePortType);

        assert(preview.status == wng::SchemaMigrationCommandPreviewStatus::Ready);
        assert(step != nullptr);
        assert(step->from_type == "number");
        assert(step->to_type == "scalar");
        assert(step->affected_ports.size() == 1);
        assert(!step->destructive);
        assert(graph.ports()[0].type == "number");
    }

    {
        // Required-port defaults become add-port preview steps. The missing port
        // is not created by this diagnostic layer.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.push_back(input("extra", "number", true));
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId node = instantiate_schema_node(graph, source, "math.add");
        const std::size_t port_count = graph.ports().size();
        wng::SchemaMigrationPolicy policy;
        wng::RequiredPortDefaultPolicy default_policy;
        default_policy.port = port_identity("math.add", wng::PortKind::Input, "extra");
        default_policy.default_value = "0";
        policy.required_port_defaults.push_back(default_policy);

        const wng::SchemaMigrationCommandPreview preview =
            wng::preview_schema_migration_commands(graph, source, target, policy);
        const wng::SchemaMigrationCommandPreviewStep* step = find_step(
            preview,
            wng::SchemaMigrationCommandPreviewStepKind::AddRequiredPort);

        assert(preview.status == wng::SchemaMigrationCommandPreviewStatus::Ready);
        assert(step != nullptr);
        assert(step->default_value == "0");
        assert(contains_node_id(step->affected_nodes, node));
        assert(step->affected_ports.empty());
        assert(!step->destructive);
        assert(graph.ports().size() == port_count);
    }

    {
        // Acknowledged node removal produces a destructive preview step, but the
        // graph node remains present because this is not migration application.
        const wng::GraphSchema source = make_schema(make_node_definition("obsolete", false));
        const wng::GraphSchema target;
        wng::Graph graph;
        const wng::NodeId node = instantiate_schema_node(graph, source, "obsolete");
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_node_removals.push_back({ "obsolete" });

        const wng::SchemaMigrationCommandPreview preview =
            wng::preview_schema_migration_commands(graph, source, target, policy);
        const wng::SchemaMigrationCommandPreviewStep* step = find_step(
            preview,
            wng::SchemaMigrationCommandPreviewStepKind::RemoveNodesForRemovedType);

        assert(preview.status == wng::SchemaMigrationCommandPreviewStatus::Ready);
        assert(step != nullptr);
        assert(step->destructive);
        assert(contains_node_id(step->affected_nodes, node));
        assert(graph.find_node(node) != nullptr);
    }

    {
        // Acknowledged port removal produces a destructive port-removal preview
        // step while leaving the graph port intact.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.clear();
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source, "math.add");
        const std::size_t port_count = graph.ports().size();
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_port_removals.push_back(
            { port_identity("math.add", wng::PortKind::Input, "value") });

        const wng::SchemaMigrationCommandPreview preview =
            wng::preview_schema_migration_commands(graph, source, target, policy);
        const wng::SchemaMigrationCommandPreviewStep* step = find_step(
            preview,
            wng::SchemaMigrationCommandPreviewStepKind::RemovePortsForRemovedDefinition);

        assert(preview.status == wng::SchemaMigrationCommandPreviewStatus::Ready);
        assert(step != nullptr);
        assert(step->destructive);
        assert(step->affected_ports.size() == 1);
        assert(graph.ports().size() == port_count);
    }

    {
        // Valid policy can be irrelevant for the current graph. The preview should
        // not emit steps for schema changes that affect no graph objects.
        const wng::GraphSchema source = make_schema(make_node_definition("old.math.add", false));
        const wng::GraphSchema target = make_schema(make_node_definition("math.add", false));
        const wng::Graph graph;
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.math.add", "math.add" });

        const wng::SchemaMigrationCommandPreview preview =
            wng::preview_schema_migration_commands(graph, source, target, policy);

        assert(preview.result == wng::Result::Ok);
        assert(preview.status == wng::SchemaMigrationCommandPreviewStatus::Ready);
        assert(preview.steps.empty());
    }

    {
        // Step ordering follows policy category order, independent of plan action
        // ordering. This keeps future UI and apply-layer previews reproducible.
        wng::NodeDefinition source_math = make_node_definition();
        source_math.inputs.clear();
        source_math.inputs.push_back(input("lhs", "number", false));
        source_math.inputs.push_back(input("typed", "number", false));
        source_math.inputs.push_back(input("removed", "number", false));
        wng::NodeDefinition target_math = make_node_definition();
        target_math.inputs.clear();
        target_math.inputs.push_back(input("a", "number", false));
        target_math.inputs.push_back(input("typed", "scalar", false));
        target_math.inputs.push_back(input("extra", "number", true));
        const wng::GraphSchema source = make_schema(
            make_node_definition("old.type", false),
            source_math,
            make_node_definition("removed.type", false));
        const wng::GraphSchema target = make_schema(
            make_node_definition("new.type", false),
            target_math);
        wng::Graph graph;
        instantiate_schema_node(graph, source, "old.type");
        instantiate_schema_node(graph, source, "math.add");
        instantiate_schema_node(graph, source, "removed.type");
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.type", "new.type" });
        wng::PortDefinitionRenamePolicy rename;
        rename.from = port_identity("math.add", wng::PortKind::Input, "lhs");
        rename.to = port_identity("math.add", wng::PortKind::Input, "a");
        policy.port_renames.push_back(rename);
        wng::PortTypeChangePolicy type_change;
        type_change.port = port_identity("math.add", wng::PortKind::Input, "typed");
        type_change.from_type = "number";
        type_change.to_type = "scalar";
        policy.port_type_changes.push_back(type_change);
        wng::RequiredPortDefaultPolicy default_policy;
        default_policy.port = port_identity("math.add", wng::PortKind::Input, "extra");
        default_policy.default_value = "0";
        policy.required_port_defaults.push_back(default_policy);
        policy.acknowledged_node_removals.push_back({ "removed.type" });
        policy.acknowledged_port_removals.push_back(
            { port_identity("math.add", wng::PortKind::Input, "removed") });

        const wng::SchemaMigrationCommandPreview preview =
            wng::preview_schema_migration_commands(graph, source, target, policy);

        assert(preview.status == wng::SchemaMigrationCommandPreviewStatus::Ready);
        assert(preview.steps.size() == 6);
        assert(preview.steps[0].kind == wng::SchemaMigrationCommandPreviewStepKind::RenameNodeType);
        assert(preview.steps[1].kind == wng::SchemaMigrationCommandPreviewStepKind::RenamePortDefinition);
        assert(preview.steps[2].kind == wng::SchemaMigrationCommandPreviewStepKind::ChangePortType);
        assert(preview.steps[3].kind == wng::SchemaMigrationCommandPreviewStepKind::AddRequiredPort);
        assert(preview.steps[4].kind ==
            wng::SchemaMigrationCommandPreviewStepKind::RemoveNodesForRemovedType);
        assert(preview.steps[5].kind ==
            wng::SchemaMigrationCommandPreviewStepKind::RemovePortsForRemovedDefinition);
    }

    {
        // Affected IDs preserve graph storage order. The preview reports the two
        // matching rename targets in the same order they were inserted.
        const wng::GraphSchema source = make_schema(make_node_definition("old.type", false));
        const wng::GraphSchema target = make_schema(make_node_definition("new.type", false));
        wng::Graph graph;
        const wng::NodeId first = instantiate_schema_node(graph, source, "old.type");
        const wng::NodeId second = instantiate_schema_node(graph, source, "old.type");
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.type", "new.type" });

        const wng::SchemaMigrationCommandPreview preview =
            wng::preview_schema_migration_commands(graph, source, target, policy);
        const wng::SchemaMigrationCommandPreviewStep* step = find_step(
            preview,
            wng::SchemaMigrationCommandPreviewStepKind::RenameNodeType);

        assert(step != nullptr);
        assert(step->affected_nodes.size() == 2);
        assert(step->affected_nodes[0] == first);
        assert(step->affected_nodes[1] == second);
    }

    {
        // Command preview is read-only and policy-preserving. It does not call the
        // graph command layer or mutate graph/schema/policy storage.
        const wng::GraphSchema source = make_schema(make_node_definition("old.type", false));
        const wng::GraphSchema target = make_schema(make_node_definition("new.type", false));
        wng::Graph graph;
        instantiate_schema_node(graph, source, "old.type");
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.type", "new.type" });
        const std::size_t node_count = graph.nodes().size();
        const std::size_t port_count = graph.ports().size();
        const std::size_t source_definition_count = source.node_definitions().size();
        const std::size_t target_definition_count = target.node_definitions().size();
        const std::size_t policy_rename_count = policy.node_type_renames.size();

        const wng::SchemaMigrationCommandPreview preview =
            wng::preview_schema_migration_commands(graph, source, target, policy);

        assert(preview.result == wng::Result::Ok);
        assert(graph.nodes().size() == node_count);
        assert(graph.ports().size() == port_count);
        assert(source.node_definitions().size() == source_definition_count);
        assert(target.node_definitions().size() == target_definition_count);
        assert(policy.node_type_renames.size() == policy_rename_count);
    }

    return 0;
}
