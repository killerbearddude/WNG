// Exercises atomic schema migration application.
// Tests verify DTO working-copy mutation, target validation, diff reporting,
// destructive cleanup, and the no-mutation guarantee on failure.

#include <cassert>
#include <string>
#include <vector>

#include <wng/graph.hpp>
#include <wng/graph_diff.hpp>
#include <wng/graph_validation.hpp>
#include <wng/schema_migration_apply.hpp>
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

    wng::NodeDefinition node_definition(
        const std::string& type,
        bool with_ports = true)
    {
        wng::NodeDefinition definition;
        definition.type = type;
        definition.display_name = type;
        if (with_ports) {
            definition.inputs.push_back(input("value", "number", true));
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

    wng::NodeDesc node_desc(const std::string& type)
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = type;
        desc.size = { 100.0f, 60.0f };
        return desc;
    }

    wng::NodeId instantiate_node(
        wng::Graph& graph,
        const wng::GraphSchema& schema,
        const std::string& type)
    {
        wng::NodeId node;
        assert(wng::instantiate_node(graph, schema, node_desc(type), &node, nullptr) ==
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

    wng::SchemaMigrationPolicy rename_node_policy(
        const std::string& from,
        const std::string& to)
    {
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ from, to });
        return policy;
    }

    wng::GraphDiff diff_from_copy(const wng::Graph& before, const wng::Graph& after)
    {
        return wng::diff_graphs(before, after);
    }

    void assert_graph_unchanged(const wng::Graph& before, const wng::Graph& after)
    {
        const wng::GraphDiff diff = diff_from_copy(before, after);
        assert(diff.result == wng::Result::Ok);
        assert(diff.empty());
    }
}

int main()
{
    {
        // Identical schemas are the no-op baseline. Apply succeeds without
        // inventing command records or changing graph identity.
        const wng::GraphSchema schema = make_schema(node_definition("math.add"));
        wng::Graph graph;
        instantiate_node(graph, schema, "math.add");
        const wng::Graph before = graph;
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, schema, schema, policy);

        assert(result.result == wng::Result::Ok);
        assert(result.status == wng::SchemaMigrationApplyStatus::Applied);
        assert(result.success());
        assert(result.applied());
        assert(result.applied_steps.empty());
        assert(result.diff.empty());
        assert_graph_unchanged(before, graph);
    }

    {
        // Structurally invalid policy fails during preview and must not mutate the
        // graph. This keeps migration application behind policy validation.
        const wng::GraphSchema schema = make_schema(node_definition("math.add"));
        wng::Graph graph;
        instantiate_node(graph, schema, "math.add");
        const wng::Graph before = graph;
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "math.add", "math.add" });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, schema, schema, policy);

        assert(result.result == wng::Result::InvalidArgument);
        assert(result.status == wng::SchemaMigrationApplyStatus::PreviewFailed);
        assert_graph_unchanged(before, graph);
    }

    {
        // Uncovered blocking actions are not guessed by apply. A removed used node
        // type without policy remains NotReady and the graph is unchanged.
        const wng::GraphSchema source = make_schema(node_definition("old.utility", false));
        const wng::GraphSchema target;
        wng::Graph graph;
        instantiate_node(graph, source, "old.utility");
        const wng::Graph before = graph;
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.result == wng::Result::InvalidArgument);
        assert(result.status == wng::SchemaMigrationApplyStatus::NotReady);
        assert_graph_unchanged(before, graph);
    }

    {
        // Node type migration rewrites stable type metadata while preserving the
        // NodeId and all existing port IDs.
        const wng::GraphSchema source = make_schema(node_definition("old.utility", false));
        const wng::GraphSchema target = make_schema(node_definition("utility", false));
        wng::Graph graph;
        const wng::NodeId node = instantiate_node(graph, source, "old.utility");
        const wng::SchemaMigrationPolicy policy = rename_node_policy("old.utility", "utility");

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::Applied);
        assert(result.diff.changed());
        assert(!result.diff.nodes.empty());
        const wng::Node* migrated = graph.find_node(node);
        assert(migrated != nullptr);
        assert(migrated->type == "utility");
        assert(wng::validate_graph(graph, target).valid());
    }

    {
        // Port rename migration changes the schema-facing port name without
        // changing the stable PortId or node input vector membership.
        const wng::GraphSchema source = make_schema(node_definition("math.add"));
        wng::NodeDefinition target_definition = node_definition("math.add");
        target_definition.inputs[0] = input("a", "number", false);
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId node = instantiate_node(graph, source, "math.add");
        const wng::Port* original = find_port(graph, node, wng::PortKind::Input, "value");
        assert(original != nullptr);
        wng::SchemaMigrationPolicy policy;
        policy.port_renames.push_back({
            port_identity("math.add", wng::PortKind::Input, "value"),
            port_identity("math.add", wng::PortKind::Input, "a") });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::Applied);
        const wng::Port* renamed = find_port(graph, node, wng::PortKind::Input, "a");
        assert(renamed != nullptr);
        assert(renamed->id == original->id);
        assert(graph.find_node(node)->inputs[0] == original->id);
        assert(wng::validate_graph(graph, target).valid());
    }

    {
        // Port type migration rewrites metadata only. Links are not touched by this
        // operation; target validation decides whether existing links remain valid.
        const wng::GraphSchema source = make_schema(node_definition("math.add"));
        wng::NodeDefinition target_definition = node_definition("math.add");
        target_definition.inputs[0].type = "scalar";
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId node = instantiate_node(graph, source, "math.add");
        const wng::Port* original = find_port(graph, node, wng::PortKind::Input, "value");
        assert(original != nullptr);
        wng::SchemaMigrationPolicy policy;
        policy.port_type_changes.push_back({
            port_identity("math.add", wng::PortKind::Input, "value"),
            "number",
            "scalar" });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::Applied);
        const wng::Port* changed = graph.find_port(original->id);
        assert(changed != nullptr);
        assert(changed->type == "scalar");
        assert(wng::validate_graph(graph, target).valid());
    }

    {
        // Required input defaults create missing structural ports with deterministic
        // new IDs. The default text remains policy/result metadata, not graph data.
        const wng::GraphSchema source = make_schema(node_definition("math.add"));
        wng::NodeDefinition target_definition = node_definition("math.add");
        target_definition.inputs.push_back(input("extra", "number", true));
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId node = instantiate_node(graph, source, "math.add");
        const std::size_t old_port_count = graph.ports().size();
        wng::SchemaMigrationPolicy policy;
        policy.required_port_defaults.push_back({
            port_identity("math.add", wng::PortKind::Input, "extra"),
            "0" });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::Applied);
        assert(graph.ports().size() == old_port_count + 1U);
        const wng::Port* extra = find_port(graph, node, wng::PortKind::Input, "extra");
        assert(extra != nullptr);
        assert(extra->type == "number");
        assert(extra->id.value > 0U);
        assert(wng::validate_graph(graph, target).valid());
        assert(result.command_preview.steps[0].default_value == "0");
    }

    {
        // Required output defaults append missing output ports to the owning node's
        // outputs list without creating links or runtime values.
        const wng::GraphSchema source = make_schema(node_definition("math.add"));
        wng::NodeDefinition target_definition = node_definition("math.add");
        target_definition.outputs.push_back(output("debug", "number", true));
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId node = instantiate_node(graph, source, "math.add");
        wng::SchemaMigrationPolicy policy;
        policy.required_port_defaults.push_back({
            port_identity("math.add", wng::PortKind::Output, "debug"),
            "0" });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::Applied);
        const wng::Port* debug = find_port(graph, node, wng::PortKind::Output, "debug");
        assert(debug != nullptr);
        assert(graph.find_node(node)->outputs.back() == debug->id);
        assert(wng::validate_graph(graph, target).valid());
    }

    {
        // Policy-covered destructive node removal now applies atomically. The
        // migrated graph contains no removed nodes or owned ports.
        const wng::GraphSchema source = make_schema(node_definition("obsolete", false));
        const wng::GraphSchema target;
        wng::Graph graph;
        const wng::NodeId obsolete = instantiate_node(graph, source, "obsolete");
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_node_removals.push_back({ "obsolete" });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.result == wng::Result::Ok);
        assert(result.status == wng::SchemaMigrationApplyStatus::Applied);
        assert(result.applied_steps.size() == 1U);
        assert(result.applied_steps[0].preview_step.destructive);
        assert(graph.find_node(obsolete) == nullptr);
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
        assert(result.diff.changed());
        assert(wng::validate_graph(graph, target).valid());
    }

    {
        // Policy-covered destructive port removal erases the port from graph
        // storage and from the owning node's input/output vectors.
        const wng::GraphSchema source = make_schema(node_definition("math.add"));
        wng::NodeDefinition target_definition = node_definition("math.add");
        target_definition.inputs.clear();
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        const wng::NodeId node = instantiate_node(graph, source, "math.add");
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_port_removals.push_back({
            port_identity("math.add", wng::PortKind::Input, "value") });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::Applied);
        assert(find_port(graph, node, wng::PortKind::Input, "value") == nullptr);
        assert(graph.find_node(node)->inputs.empty());
        assert(result.diff.changed());
        assert(wng::validate_graph(graph, target).valid());
    }

    {
        // Port kind changes require moving IDs between node input/output lists and
        // are rejected until that behavior is designed explicitly.
        wng::NodeDefinition source_definition;
        source_definition.type = "math.add";
        source_definition.display_name = "math.add";
        source_definition.inputs.push_back(input("lhs", "number", true));
        const wng::GraphSchema source = make_schema(source_definition);
        wng::NodeDefinition target_definition;
        target_definition.type = "math.add";
        target_definition.display_name = "math.add";
        target_definition.outputs.push_back(output("a", "number", false));
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_node(graph, source, "math.add");
        const wng::Graph before = graph;
        wng::SchemaMigrationPolicy policy;
        policy.port_renames.push_back({
            port_identity("math.add", wng::PortKind::Input, "lhs"),
            port_identity("math.add", wng::PortKind::Output, "a") });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.result == wng::Result::InvalidArgument);
        assert(result.status == wng::SchemaMigrationApplyStatus::NotReady);
        assert_graph_unchanged(before, graph);
    }

    {
        // Target validation failure after DTO migration keeps the original graph.
        // Here the target node type exists but is disabled, so schema validation
        // rejects it with the concrete validation issue result.
        const wng::GraphSchema source = make_schema(node_definition("old.utility", false));
        wng::NodeDefinition target_definition = node_definition("utility", false);
        target_definition.enabled = false;
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_node(graph, source, "old.utility");
        const wng::Graph before = graph;
        const wng::SchemaMigrationPolicy policy = rename_node_policy("old.utility", "utility");

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::TargetValidationFailed);
        assert(result.result == wng::Result::InvalidConnection);
        assert_graph_unchanged(before, graph);
    }

    {
        // Multiple non-destructive operations apply in command-preview order:
        // node type rename, port rename, port type change, then required port add.
        const wng::GraphSchema source = make_schema(
            node_definition("old.utility", false),
            node_definition("math.add"));
        wng::NodeDefinition utility_target = node_definition("utility", false);
        wng::NodeDefinition math_target = node_definition("math.add");
        math_target.inputs[0] = input("a", "number", false);
        math_target.inputs.push_back(input("extra", "number", true));
        math_target.outputs[0].type = "scalar";
        const wng::GraphSchema target = make_schema(utility_target, math_target);
        wng::Graph graph;
        const wng::NodeId utility = instantiate_node(graph, source, "old.utility");
        const wng::NodeId math = instantiate_node(graph, source, "math.add");
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.utility", "utility" });
        policy.port_renames.push_back({
            port_identity("math.add", wng::PortKind::Input, "value"),
            port_identity("math.add", wng::PortKind::Input, "a") });
        policy.port_type_changes.push_back({
            port_identity("math.add", wng::PortKind::Output, "result"),
            "number",
            "scalar" });
        policy.required_port_defaults.push_back({
            port_identity("math.add", wng::PortKind::Input, "extra"),
            "0" });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::Applied);
        assert(result.applied_steps.size() == 4U);
        assert(graph.find_node(utility)->type == "utility");
        assert(find_port(graph, math, wng::PortKind::Input, "a") != nullptr);
        assert(find_port(graph, math, wng::PortKind::Input, "extra") != nullptr);
        assert(find_port(graph, math, wng::PortKind::Output, "result")->type == "scalar");
        assert(wng::validate_graph(graph, target).valid());
    }

    {
        // Apply results preserve the command preview and final diff so future
        // command/history integration can wrap successful migrations later.
        const wng::GraphSchema source = make_schema(node_definition("math.add"));
        wng::NodeDefinition target_definition = node_definition("math.add");
        target_definition.inputs[0] = input("a", "number", false);
        const wng::GraphSchema target = make_schema(target_definition);
        wng::Graph graph;
        instantiate_node(graph, source, "math.add");
        wng::SchemaMigrationPolicy policy;
        policy.port_renames.push_back({
            port_identity("math.add", wng::PortKind::Input, "value"),
            port_identity("math.add", wng::PortKind::Input, "a") });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::Applied);
        assert(!result.command_preview.steps.empty());
        assert(result.applied_steps.size() == result.command_preview.steps.size());
        assert(result.diff.changed());
    }

    {
        // Atomicity holds even if a later step fails after an earlier DTO rewrite.
        // The original graph is not replaced until every check succeeds.
        const wng::GraphSchema source = make_schema(
            node_definition("old.utility", false),
            [] {
                wng::NodeDefinition definition;
                definition.type = "math.add";
                definition.display_name = "math.add";
                definition.inputs.push_back(input("lhs", "number", true));
                return definition;
            }());
        const wng::GraphSchema target = make_schema(
            node_definition("utility", false),
            [] {
                wng::NodeDefinition definition;
                definition.type = "math.add";
                definition.display_name = "math.add";
                definition.outputs.push_back(output("a", "number", false));
                return definition;
            }());
        wng::Graph graph;
        instantiate_node(graph, source, "old.utility");
        instantiate_node(graph, source, "math.add");
        const wng::Graph before = graph;
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.utility", "utility" });
        policy.port_renames.push_back({
            port_identity("math.add", wng::PortKind::Input, "lhs"),
            port_identity("math.add", wng::PortKind::Output, "a") });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::NotReady);
        assert(result.result == wng::Result::InvalidArgument);
        assert_graph_unchanged(before, graph);
    }

    return 0;
}
