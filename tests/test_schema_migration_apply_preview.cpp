// Exercises read-only schema migration apply previews.
// Tests verify policy validation integration, policy-aware planning buckets,
// readiness classification, and the no-mutation guarantee.

#include <cassert>
#include <string>
#include <vector>

#include <wng/graph.hpp>
#include <wng/schema_migration_apply_preview.hpp>
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
        const std::string& type = "math.add")
    {
        wng::NodeDefinition definition;
        definition.type = type;
        definition.display_name = type;
        definition.inputs.push_back(input("value", "number", true));
        definition.outputs.push_back(output("result", "number", false));
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

    bool actions_contain_kind(
        const std::vector<wng::SchemaMigrationAction>& actions,
        wng::SchemaMigrationActionKind kind)
    {
        for (const wng::SchemaMigrationAction& action : actions) {
            if (action.kind == kind) {
                return true;
            }
        }

        return false;
    }

    bool actions_contain(
        const std::vector<wng::SchemaMigrationAction>& actions,
        wng::SchemaMigrationActionKind kind,
        const std::string& node_type,
        const std::string& port_name = "")
    {
        for (const wng::SchemaMigrationAction& action : actions) {
            if (action.kind == kind &&
                action.node_type == node_type &&
                (port_name.empty() || action.port_name == port_name)) {
                return true;
            }
        }

        return false;
    }
}

int main()
{
    {
        // Empty schemas and an empty graph are the apply-preview no-op baseline.
        // No policy coverage or migration action is needed.
        const wng::Graph graph;
        const wng::GraphSchema source;
        const wng::GraphSchema target;
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, source, target, policy);

        assert(preview.result == wng::Result::Ok);
        assert(preview.status == wng::SchemaMigrationApplyPreviewStatus::Ready);
        assert(preview.success());
        assert(preview.ready());
        assert(!preview.blocked());
        assert(preview.uncovered_blocking_actions.empty());
        assert(preview.covered_blocking_actions.empty());
        assert(preview.non_blocking_actions.empty());
    }

    {
        // A graph already valid under identical schemas is ready. This protects
        // the compatible fast path for future migration application callers.
        const wng::GraphSchema schema = make_schema(make_node_definition());
        wng::Graph graph;
        instantiate_schema_node(graph, schema);
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, schema, schema, policy);

        assert(preview.status == wng::SchemaMigrationApplyPreviewStatus::Ready);
        assert(preview.ready());
        assert(preview.plan.actions.empty());
    }

    {
        // Structurally invalid policy stops preview before planning. A future apply
        // layer must never consume self-renames or other ambiguous policy data.
        const wng::GraphSchema schema = make_schema(make_node_definition());
        const wng::Graph graph;
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "math.add", "math.add" });

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, schema, schema, policy);

        assert(preview.status == wng::SchemaMigrationApplyPreviewStatus::PolicyInvalid);
        assert(preview.result == wng::Result::InvalidArgument);
        assert(!preview.policy_validation.valid());
        assert(preview.plan.actions.empty());
    }

    {
        // Schema-aware policy validation catches references to missing source
        // types before those policies can be mistaken for usable coverage.
        const wng::Graph graph;
        const wng::GraphSchema source;
        const wng::GraphSchema target = make_schema(make_node_definition());
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "missing.node", "math.add" });

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, source, target, policy);

        assert(preview.status == wng::SchemaMigrationApplyPreviewStatus::PolicyInvalid);
        assert(preview.result == wng::Result::NotFound);
        assert(!preview.policy_validation.issues.empty());
    }

    {
        // Removing a used node type without policy is an uncovered blocking action.
        // Preview reports the blocker without deleting graph nodes.
        const wng::GraphSchema source = make_schema(make_node_definition());
        const wng::GraphSchema target;
        wng::Graph graph;
        instantiate_schema_node(graph, source);
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, source, target, policy);

        assert(preview.status ==
            wng::SchemaMigrationApplyPreviewStatus::BlockedByUncoveredActions);
        assert(preview.result == wng::Result::Ok);
        assert(!preview.ready());
        assert(preview.blocked());
        assert(actions_contain_kind(
            preview.uncovered_blocking_actions,
            wng::SchemaMigrationActionKind::RemoveNodeType));
        assert(preview.covered_blocking_actions.empty());
    }

    {
        // Policy coverage is not migration application. Removing a node type also
        // removes its schema-owned port definitions, so this test acknowledges all
        // generated blocking actions while still requiring validation to block
        // readiness until a future apply layer mutates the graph.
        const wng::GraphSchema source = make_schema(make_node_definition());
        const wng::GraphSchema target;
        wng::Graph graph;
        instantiate_schema_node(graph, source);
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_node_removals.push_back({ "math.add" });
        policy.acknowledged_port_removals.push_back(
            { port_identity("math.add", wng::PortKind::Input, "value") });
        policy.acknowledged_port_removals.push_back(
            { port_identity("math.add", wng::PortKind::Output, "result") });

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, source, target, policy);

        assert(preview.policy_validation.valid());
        assert(actions_contain_kind(
            preview.covered_blocking_actions,
            wng::SchemaMigrationActionKind::RemoveNodeType));
        assert(preview.uncovered_blocking_actions.empty());
        assert(preview.status == wng::SchemaMigrationApplyPreviewStatus::BlockedByValidation);
        assert(!preview.ready());
    }

    {
        // A newly required port without a default policy is uncovered. The preview
        // does not create the missing port; it only reports why apply is not ready.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.push_back(input("extra", "number", true));
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source);
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, source, target, policy);

        assert(preview.status ==
            wng::SchemaMigrationApplyPreviewStatus::BlockedByUncoveredActions);
        assert(actions_contain(
            preview.uncovered_blocking_actions,
            wng::SchemaMigrationActionKind::AddRequiredPort,
            "math.add",
            "extra"));
    }

    {
        // A required-port default covers the action, but the graph is still
        // target-invalid until a future apply layer actually creates the port.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.push_back(input("extra", "number", true));
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId node = instantiate_schema_node(graph, source);
        wng::SchemaMigrationPolicy policy;
        policy.required_port_defaults.push_back(
            { port_identity("math.add", wng::PortKind::Input, "extra"), "0" });

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, source, target, policy);

        assert(actions_contain(
            preview.covered_blocking_actions,
            wng::SchemaMigrationActionKind::AddRequiredPort,
            "math.add",
            "extra"));
        assert(preview.uncovered_blocking_actions.empty());
        assert(preview.status == wng::SchemaMigrationApplyPreviewStatus::BlockedByValidation);
        assert(find_port(graph, node, wng::PortKind::Input, "extra") == nullptr);
    }

    {
        // Added optional ports do not force graph edits. The preview remains ready
        // because missing optional ports are compatible with the target schema.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.push_back(input("optional", "number", false));
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source);
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, source, target, policy);

        assert(preview.status == wng::SchemaMigrationApplyPreviewStatus::Ready);
        assert(preview.ready());
        assert(preview.uncovered_blocking_actions.empty());
    }

    {
        // Removing a graph-backed port without policy is an uncovered blocker. The
        // preview must not remove the port while reporting the migration risk.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.clear();
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId node = instantiate_schema_node(graph, source);
        assert(find_port(graph, node, wng::PortKind::Input, "value") != nullptr);
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, source, target, policy);

        assert(actions_contain(
            preview.uncovered_blocking_actions,
            wng::SchemaMigrationActionKind::RemovePortDefinition,
            "math.add",
            "value"));
        assert(preview.status ==
            wng::SchemaMigrationApplyPreviewStatus::BlockedByUncoveredActions);
        assert(find_port(graph, node, wng::PortKind::Input, "value") != nullptr);
    }

    {
        // Port-removal acknowledgement moves the action into the covered bucket,
        // but readiness is still blocked until a future migration applies it.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.clear();
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source);
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_port_removals.push_back(
            { port_identity("math.add", wng::PortKind::Input, "value") });

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, source, target, policy);

        assert(actions_contain(
            preview.covered_blocking_actions,
            wng::SchemaMigrationActionKind::RemovePortDefinition,
            "math.add",
            "value"));
        assert(preview.uncovered_blocking_actions.empty());
        assert(preview.status == wng::SchemaMigrationApplyPreviewStatus::BlockedByValidation);
    }

    {
        // Port type change policies cover matching modify-port actions by stable
        // identity. Current validation makes the action blocking, so it should be
        // reported as covered blocking rather than applied.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs[0].type = "scalar";
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source);
        wng::SchemaMigrationPolicy policy;
        policy.port_type_changes.push_back(
            { port_identity("math.add", wng::PortKind::Input, "value"), "number", "scalar" });

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, source, target, policy);

        assert(preview.policy_validation.valid());
        assert(actions_contain(
            preview.covered_blocking_actions,
            wng::SchemaMigrationActionKind::ModifyPortDefinition,
            "math.add",
            "value") ||
            actions_contain(
                preview.non_blocking_actions,
                wng::SchemaMigrationActionKind::ModifyPortDefinition,
                "math.add",
                "value"));
    }

    {
        // Bucket classification preserves original migration action order within
        // each bucket. UI diagnostics can display these vectors without sorting.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs[0].type = "scalar";
        target_definition.inputs.push_back(input("extra", "number", true));
        target_definition.outputs.clear();
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source);
        wng::SchemaMigrationPolicy policy;
        policy.port_type_changes.push_back(
            { port_identity("math.add", wng::PortKind::Input, "value"), "number", "scalar" });
        policy.required_port_defaults.push_back(
            { port_identity("math.add", wng::PortKind::Input, "extra"), "0" });

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, source, target, policy);

        assert(preview.covered_blocking_actions.size() >= 2);
        assert(preview.covered_blocking_actions[0].kind ==
            wng::SchemaMigrationActionKind::ModifyPortDefinition);
        assert(preview.covered_blocking_actions[1].kind ==
            wng::SchemaMigrationActionKind::AddRequiredPort);
    }

    {
        // Preview is read-only across all inputs. This protects the boundary
        // between diagnostics and a future explicit migration application layer.
        const wng::GraphSchema source = make_schema(make_node_definition());
        wng::NodeDefinition target_definition = make_node_definition();
        target_definition.inputs.push_back(input("extra", "number", true));
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_schema_node(graph, source);
        wng::SchemaMigrationPolicy policy;
        policy.required_port_defaults.push_back(
            { port_identity("math.add", wng::PortKind::Input, "extra"), "0" });
        const std::size_t node_count = graph.nodes().size();
        const std::size_t port_count = graph.ports().size();
        const std::size_t source_definition_count = source.node_definitions().size();
        const std::size_t target_definition_count = target.node_definitions().size();
        const std::size_t policy_default_count = policy.required_port_defaults.size();

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, source, target, policy);

        assert(preview.result == wng::Result::Ok);
        assert(graph.nodes().size() == node_count);
        assert(graph.ports().size() == port_count);
        assert(source.node_definitions().size() == source_definition_count);
        assert(target.node_definitions().size() == target_definition_count);
        assert(policy.required_port_defaults.size() == policy_default_count);
    }

    {
        // Preview stores both the schema-aware policy validation and the full plan
        // so future callers can inspect diagnostics without recomputing them.
        const wng::GraphSchema source = make_schema(make_node_definition());
        const wng::GraphSchema target;
        wng::Graph graph;
        instantiate_schema_node(graph, source);
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationApplyPreview preview =
            wng::preview_schema_migration_application(graph, source, target, policy);

        assert(preview.policy_validation.result == wng::Result::Ok);
        assert(preview.plan.result == wng::Result::Ok);
        assert(!preview.plan.actions.empty());
    }

    return 0;
}
