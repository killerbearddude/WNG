// Exercises rejected schema migration history record invariants.
// Invalid migration records must not disturb existing undo/redo stacks or graph
// state owned by earlier valid migration records.

#include <cassert>
#include <initializer_list>
#include <string>

#include <wng/graph.hpp>
#include <wng/schema.hpp>
#include <wng/schema_migration_apply_command.hpp>
#include <wng/schema_migration_apply_command_history.hpp>
#include <wng/schema_migration_policy.hpp>

namespace
{
    wng::NodeDefinition node_definition(const std::string& type)
    {
        wng::NodeDefinition definition;
        definition.type = type;
        definition.display_name = type;
        return definition;
    }

    wng::GraphSchema make_schema(
        std::initializer_list<wng::NodeDefinition> definitions)
    {
        wng::GraphSchema schema;
        for (const wng::NodeDefinition& definition : definitions) {
            assert(schema.add_node_definition(definition) == wng::Result::Ok);
        }
        return schema;
    }

    wng::NodeId create_node(wng::Graph& graph, const std::string& type)
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = type;

        wng::NodeId id;
        assert(graph.create_node(desc, &id) == wng::Result::Ok);
        return id;
    }

    const wng::Node& require_node(const wng::Graph& graph, wng::NodeId id)
    {
        const wng::Node* node = graph.find_node(id);
        assert(node != nullptr);
        return *node;
    }

    wng::SchemaMigrationPolicy rename_policy(
        const std::string& from,
        const std::string& to)
    {
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ from, to });
        return policy;
    }

    wng::SchemaMigrationApplyCommandResult apply_rename_command(
        wng::Graph& graph,
        const std::string& from,
        const std::string& to)
    {
        const wng::GraphSchema source = make_schema({ node_definition(from) });
        const wng::GraphSchema target = make_schema({ node_definition(to) });
        return wng::command_apply_schema_migration(
            graph,
            source,
            target,
            rename_policy(from, to));
    }

    wng::SchemaMigrationApplyCommandResult apply_without_policy(
        wng::Graph& graph,
        const std::string& from,
        const std::string& to)
    {
        const wng::GraphSchema source = make_schema({ node_definition(from) });
        const wng::GraphSchema target = make_schema({ node_definition(to) });
        const wng::SchemaMigrationPolicy empty_policy;
        return wng::command_apply_schema_migration(
            graph,
            source,
            target,
            empty_policy);
    }
}

int main()
{
    wng::Graph graph;
    const wng::NodeId node = create_node(graph, "legacy.node");

    const wng::SchemaMigrationApplyCommandResult command =
        apply_rename_command(graph, "legacy.node", "modern.node");
    assert(command.success());
    assert(require_node(graph, node).type == "modern.node");

    wng::SchemaMigrationApplyCommandHistory history;
    assert(history.record(command.record) == wng::Result::Ok);
    assert(history.can_undo());
    assert(!history.can_redo());

    const wng::SchemaMigrationApplyCommandHistoryResult undo =
        wng::undo_last_schema_migration_apply_command(graph, history);
    assert(undo.success());
    assert(require_node(graph, node).type == "legacy.node");
    assert(!history.can_undo());
    assert(history.can_redo());
    assert(history.undo_count() == 0U);
    assert(history.redo_count() == 1U);

    const wng::SchemaMigrationApplyCommandResult failed =
        apply_without_policy(graph, "legacy.node", "modern.node");
    assert(!failed.success());
    assert(failed.status == wng::SchemaMigrationApplyCommandStatus::ApplyFailed);
    assert(require_node(graph, node).type == "legacy.node");

    assert(history.record(failed.record) == wng::Result::InvalidArgument);
    assert(!history.can_undo());
    assert(history.can_redo());
    assert(history.undo_count() == 0U);
    assert(history.redo_count() == 1U);
    assert(require_node(graph, node).type == "legacy.node");

    const wng::SchemaMigrationApplyCommandHistoryResult redo =
        wng::redo_last_schema_migration_apply_command(graph, history);
    assert(redo.success());
    assert(require_node(graph, node).type == "modern.node");
    assert(history.can_undo());
    assert(!history.can_redo());
    assert(history.undo_count() == 1U);
    assert(history.redo_count() == 0U);

    return 0;
}
