// Exercises graph-level command wrapping for schema migration application.
// Tests verify before/after snapshot capture, apply-result preservation, diff
// preservation, failure reporting, and manual snapshot restore for future history.

#include <cassert>
#include <initializer_list>
#include <string>
#include <vector>

#include <wng/graph.hpp>
#include <wng/graph_diff.hpp>
#include <wng/graph_snapshot.hpp>
#include <wng/graph_validation.hpp>
#include <wng/schema_migration_apply_command.hpp>
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

    wng::GraphSchema make_schema(std::initializer_list<wng::NodeDefinition> definitions)
    {
        wng::GraphSchema schema;
        for (const wng::NodeDefinition& definition : definitions) {
            assert(schema.add_node_definition(definition) == wng::Result::Ok);
        }
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

    void assert_graph_unchanged(const wng::Graph& before, const wng::Graph& after)
    {
        const wng::GraphDiff diff = wng::diff_graphs(before, after);
        assert(diff.result == wng::Result::Ok);
        assert(diff.empty());
    }

    wng::Graph restore_snapshot(const wng::GraphSnapshot& snapshot)
    {
        wng::Graph graph;
        assert(wng::restore_graph_snapshot(graph, snapshot) == wng::Result::Ok);
        return graph;
    }

    bool diff_has_removed_node(const wng::GraphDiff& diff, wng::NodeId id)
    {
        for (const wng::NodeDiff& node : diff.nodes) {
            if (node.change == wng::GraphDiffChange::Removed && node.id == id) {
                return true;
            }
        }
        return false;
    }
}

int main()
{
    {
        // Default records are empty so failed or uninitialized command values can
        // be distinguished from successfully captured migration records.
        const wng::SchemaMigrationApplyCommandRecord record;
        assert(record.empty());
    }

    {
        // A no-op migration still captures before/after graph snapshots. Future
        // history integration can represent the completed command boundary even
        // when the migration does not change graph structure.
        const wng::GraphSchema schema = make_schema({ node_definition("math.add") });
        wng::Graph graph;
        instantiate_node(graph, schema, "math.add");
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationApplyCommandResult result =
            wng::command_apply_schema_migration(graph, schema, schema, policy);

        assert(result.result == wng::Result::Ok);
        assert(result.status == wng::SchemaMigrationApplyCommandStatus::Applied);
        assert(result.success());
        assert(!result.record.empty());
        assert(!result.record.before.empty());
        assert(!result.record.after.empty());
        assert(result.record.apply_result.status == wng::SchemaMigrationApplyStatus::Applied);
        assert(result.record.diff.empty());
    }

    {
        // Node type migration command records must preserve both pre- and
        // post-migration states. Future undo can restore the old node type from
        // the before snapshot, while redo can restore the migrated type.
        const wng::GraphSchema source = make_schema({ node_definition("old.utility", false) });
        const wng::GraphSchema target = make_schema({ node_definition("utility", false) });
        wng::Graph graph;
        const wng::NodeId node = instantiate_node(graph, source, "old.utility");
        const wng::SchemaMigrationPolicy policy =
            rename_node_policy("old.utility", "utility");

        const wng::SchemaMigrationApplyCommandResult result =
            wng::command_apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyCommandStatus::Applied);
        assert(result.record.diff.changed());
        assert(graph.find_node(node)->type == "utility");

        const wng::Graph before = restore_snapshot(result.record.before);
        const wng::Graph after = restore_snapshot(result.record.after);
        assert(before.find_node(node)->type == "old.utility");
        assert(after.find_node(node)->type == "utility");
    }

    {
        // Port rename command records preserve metadata rewrites in snapshots
        // without requiring GraphCommandRecord to grow a port-name mutation kind.
        const wng::GraphSchema source = make_schema({ node_definition("math.add") });
        wng::NodeDefinition target_definition = node_definition("math.add");
        target_definition.inputs[0].name = "a";
        const wng::GraphSchema target = make_schema({ target_definition });
        wng::Graph graph;
        const wng::NodeId node = instantiate_node(graph, source, "math.add");
        wng::SchemaMigrationPolicy policy;
        policy.port_renames.push_back({
            port_identity("math.add", wng::PortKind::Input, "value"),
            port_identity("math.add", wng::PortKind::Input, "a") });

        const wng::SchemaMigrationApplyCommandResult result =
            wng::command_apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyCommandStatus::Applied);
        const wng::Graph before = restore_snapshot(result.record.before);
        const wng::Graph after = restore_snapshot(result.record.after);
        assert(find_port(before, node, wng::PortKind::Input, "value") != nullptr);
        assert(find_port(after, node, wng::PortKind::Input, "a") != nullptr);
    }

    {
        // Destructive migration command records capture removed objects in the
        // before snapshot and absence in the after snapshot. This is the minimum
        // graph-level boundary future undo/redo needs for destructive migrations.
        const wng::GraphSchema source = make_schema({
            node_definition("obsolete", false),
            node_definition("survivor", false) });
        const wng::GraphSchema target = make_schema({ node_definition("survivor", false) });
        wng::Graph graph;
        const wng::NodeId obsolete = instantiate_node(graph, source, "obsolete");
        const wng::NodeId survivor = instantiate_node(graph, source, "survivor");
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_node_removals.push_back({ "obsolete" });

        const wng::SchemaMigrationApplyCommandResult result =
            wng::command_apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyCommandStatus::Applied);
        assert(diff_has_removed_node(result.record.diff, obsolete));
        assert(graph.find_node(obsolete) == nullptr);
        assert(graph.find_node(survivor) != nullptr);

        const wng::Graph before = restore_snapshot(result.record.before);
        const wng::Graph after = restore_snapshot(result.record.after);
        assert(before.find_node(obsolete) != nullptr);
        assert(after.find_node(obsolete) == nullptr);
        assert(after.find_node(survivor) != nullptr);
    }

    {
        // Apply failures are reported at the wrapper boundary and the graph remains
        // unchanged because apply_schema_migration is already failure-atomic.
        const wng::GraphSchema source = make_schema({ node_definition("obsolete", false) });
        const wng::GraphSchema target;
        wng::Graph graph;
        instantiate_node(graph, source, "obsolete");
        const wng::Graph before = graph;
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationApplyCommandResult result =
            wng::command_apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyCommandStatus::ApplyFailed);
        assert(result.result != wng::Result::Ok);
        assert(!result.record.before.empty());
        assert(result.record.after.empty());
        assert(result.record.apply_result.result != wng::Result::Ok);
        assert_graph_unchanged(before, graph);
    }

    {
        // Invalid policy data fails through the wrapped apply result rather than
        // partially constructing a successful command record.
        const wng::GraphSchema schema = make_schema({ node_definition("math.add") });
        wng::Graph graph;
        instantiate_node(graph, schema, "math.add");
        const wng::Graph before = graph;
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "math.add", "math.add" });

        const wng::SchemaMigrationApplyCommandResult result =
            wng::command_apply_schema_migration(graph, schema, schema, policy);

        assert(result.status == wng::SchemaMigrationApplyCommandStatus::ApplyFailed);
        assert(result.record.apply_result.status ==
            wng::SchemaMigrationApplyStatus::PreviewFailed);
        assert(result.record.after.empty());
        assert_graph_unchanged(before, graph);
    }

    {
        // Manual snapshot restore demonstrates the future undo/redo value without
        // integrating with GraphCommandHistory, GraphUndo, or GraphRedo in this patch.
        const wng::GraphSchema source = make_schema({ node_definition("old.utility", false) });
        const wng::GraphSchema target = make_schema({ node_definition("utility", false) });
        wng::Graph graph;
        const wng::NodeId node = instantiate_node(graph, source, "old.utility");
        const wng::Graph original = graph;
        const wng::SchemaMigrationPolicy policy =
            rename_node_policy("old.utility", "utility");

        const wng::SchemaMigrationApplyCommandResult result =
            wng::command_apply_schema_migration(graph, source, target, policy);
        const wng::Graph migrated = graph;

        assert(result.success());
        assert(wng::restore_graph_snapshot(graph, result.record.before) == wng::Result::Ok);
        assert_graph_unchanged(original, graph);
        assert(graph.find_node(node)->type == "old.utility");

        assert(wng::restore_graph_snapshot(graph, result.record.after) == wng::Result::Ok);
        assert_graph_unchanged(migrated, graph);
        assert(graph.find_node(node)->type == "utility");
        assert(wng::validate_graph(graph, target).valid());
    }

    return 0;
}
