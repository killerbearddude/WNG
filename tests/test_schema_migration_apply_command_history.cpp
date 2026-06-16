#include <cassert>
#include <initializer_list>
#include <string>

#include <wng/graph.hpp>
#include <wng/result.hpp>
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

    void duplicate_first_before_node(wng::SchemaMigrationApplyCommandRecord& record)
    {
        assert(!record.before.graph.nodes.empty());
        record.before.graph.nodes.push_back(record.before.graph.nodes.front());
    }

    void duplicate_first_after_node(wng::SchemaMigrationApplyCommandRecord& record)
    {
        assert(!record.after.graph.nodes.empty());
        record.after.graph.nodes.push_back(record.after.graph.nodes.front());
    }
}

int main()
{
    {
        // Empty history reports that no migration command can be moved. Future
        // editor integration depends on this being a non-mutating query failure.
        wng::Graph graph;
        wng::SchemaMigrationApplyCommandHistory history;

        assert(!history.can_undo());
        assert(!history.can_redo());
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);

        const wng::SchemaMigrationApplyCommandHistoryResult undo =
            wng::undo_last_schema_migration_apply_command(graph, history);
        const wng::SchemaMigrationApplyCommandHistoryResult redo =
            wng::redo_last_schema_migration_apply_command(graph, history);

        assert(!undo.success());
        assert(undo.result == wng::Result::NotFound);
        assert(!redo.success());
        assert(redo.result == wng::Result::NotFound);
    }

    {
        // Default records are rejected because they do not contain restorable
        // before/after graph states for future undo/redo use.
        wng::SchemaMigrationApplyCommandHistory history;
        const wng::SchemaMigrationApplyCommandRecord record;

        assert(record.empty());
        assert(history.record(record) == wng::Result::InvalidArgument);
        assert(!history.can_undo());
    }

    {
        // A successful migration command can be recorded, undone through its
        // before snapshot, and redone through its after snapshot without using
        // GraphCommandHistory or GraphCommandRecord replay.
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
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);

        const wng::SchemaMigrationApplyCommandHistoryResult undo =
            wng::undo_last_schema_migration_apply_command(graph, history);
        assert(undo.success());
        assert(require_node(graph, node).type == "legacy.node");
        assert(!history.can_undo());
        assert(history.can_redo());
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);

        const wng::SchemaMigrationApplyCommandHistoryResult redo =
            wng::redo_last_schema_migration_apply_command(graph, history);
        assert(redo.success());
        assert(require_node(graph, node).type == "modern.node");
        assert(history.can_undo());
        assert(!history.can_redo());
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
    }

    {
        // Recording a new migration after undo clears redo, matching normal
        // history-branch semantics while remaining migration-history-specific.
        wng::Graph graph;
        create_node(graph, "legacy.node");

        const wng::SchemaMigrationApplyCommandResult first =
            apply_rename_command(graph, "legacy.node", "modern.node");
        assert(first.success());

        wng::SchemaMigrationApplyCommandHistory history;
        assert(history.record(first.record) == wng::Result::Ok);
        assert(wng::undo_last_schema_migration_apply_command(graph, history).success());
        assert(history.can_redo());

        const wng::SchemaMigrationApplyCommandResult second =
            apply_rename_command(graph, "legacy.node", "modern.node");
        assert(second.success());
        assert(history.record(second.record) == wng::Result::Ok);

        assert(history.can_undo());
        assert(!history.can_redo());
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
    }

    {
        // Failed migration command records are rejected even if they captured a
        // before snapshot. Only successful graph transforms are safe history units.
        wng::Graph graph;
        create_node(graph, "legacy.node");

        const wng::GraphSchema source = make_schema({ node_definition("legacy.node") });
        const wng::GraphSchema target = make_schema({ node_definition("modern.node") });
        const wng::SchemaMigrationPolicy empty_policy;
        const wng::SchemaMigrationApplyCommandResult failed =
            wng::command_apply_schema_migration(graph, source, target, empty_policy);

        assert(!failed.success());
        assert(failed.status == wng::SchemaMigrationApplyCommandStatus::ApplyFailed);

        wng::SchemaMigrationApplyCommandHistory history;
        assert(history.record(failed.record) == wng::Result::InvalidArgument);
        assert(!history.can_undo());
    }

    {
        // If undo cannot restore the recorded before snapshot, the command must
        // remain undoable and must not be moved onto redo.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "legacy.node");
        wng::SchemaMigrationApplyCommandResult command =
            apply_rename_command(graph, "legacy.node", "modern.node");
        assert(command.success());
        assert(require_node(graph, node).type == "modern.node");

        duplicate_first_before_node(command.record);

        wng::SchemaMigrationApplyCommandHistory history;
        assert(history.record(command.record) == wng::Result::Ok);
        assert(history.can_undo());
        assert(!history.can_redo());
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);

        const wng::SchemaMigrationApplyCommandHistoryResult undo =
            wng::undo_last_schema_migration_apply_command(graph, history);
        assert(!undo.success());
        assert(undo.result == wng::Result::AlreadyExists);
        assert(require_node(graph, node).type == "modern.node");
        assert(history.can_undo());
        assert(!history.can_redo());
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
    }

    {
        // If redo cannot restore the recorded after snapshot, the command must
        // remain redoable and must not be moved back onto undo.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "legacy.node");
        wng::SchemaMigrationApplyCommandResult command =
            apply_rename_command(graph, "legacy.node", "modern.node");
        assert(command.success());
        assert(require_node(graph, node).type == "modern.node");

        duplicate_first_after_node(command.record);

        wng::SchemaMigrationApplyCommandHistory history;
        assert(history.record(command.record) == wng::Result::Ok);

        const wng::SchemaMigrationApplyCommandHistoryResult undo =
            wng::undo_last_schema_migration_apply_command(graph, history);
        assert(undo.success());
        assert(require_node(graph, node).type == "legacy.node");
        assert(!history.can_undo());
        assert(history.can_redo());
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);

        const wng::SchemaMigrationApplyCommandHistoryResult redo =
            wng::redo_last_schema_migration_apply_command(graph, history);
        assert(!redo.success());
        assert(redo.result == wng::Result::AlreadyExists);
        assert(require_node(graph, node).type == "legacy.node");
        assert(!history.can_undo());
        assert(history.can_redo());
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);
    }

    {
        // Explicit clearing helpers let an owning tool discard migration history
        // without coupling this stack to editor or selection state.
        wng::Graph graph;
        create_node(graph, "legacy.node");
        const wng::SchemaMigrationApplyCommandResult command =
            apply_rename_command(graph, "legacy.node", "modern.node");

        wng::SchemaMigrationApplyCommandHistory history;
        assert(history.record(command.record) == wng::Result::Ok);
        assert(wng::undo_last_schema_migration_apply_command(graph, history).success());
        assert(history.can_redo());

        history.clear_redo();
        assert(!history.can_redo());

        assert(history.record(command.record) == wng::Result::Ok);
        history.clear();
        assert(!history.can_undo());
        assert(!history.can_redo());
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    return 0;
}
