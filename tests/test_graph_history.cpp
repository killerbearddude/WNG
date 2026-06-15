// Exercises the mixed graph-level history owner for normal graph commands and
// schema migration apply commands. These tests keep editor state, selection state,
// WPL integration, and migration execution policy outside GraphHistory.

#include <cassert>
#include <initializer_list>
#include <string>
#include <vector>

#include <wng/graph_command_history.hpp>
#include <wng/graph_history.hpp>
#include <wng/schema.hpp>
#include <wng/schema_migration_apply_command_history.hpp>
#include <wng/schema_migration_policy.hpp>

namespace
{
    wng::NodeDesc make_node_desc(
        const std::string& title,
        const std::string& type = "history.node")
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = title;
        desc.position = wng::Vec2 { 1.0f, 2.0f };
        desc.size = wng::Vec2 { 100.0f, 50.0f };
        return desc;
    }

    wng::PortDesc make_port_desc(wng::PortKind kind, const char* name)
    {
        wng::PortDesc desc;
        desc.kind = kind;
        desc.name = name;
        desc.type = "number";
        return desc;
    }

    wng::GraphCommandBatch make_batch(
        const std::vector<wng::GraphCommandRecord>& records)
    {
        wng::GraphCommandBatch batch;
        batch.result = wng::Result::Ok;
        batch.records = records;
        return batch;
    }

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

    wng::NodeId create_node_direct(wng::Graph& graph, const std::string& type)
    {
        wng::NodeId id;
        assert(graph.create_node(make_node_desc(type, type), &id) == wng::Result::Ok);
        return id;
    }

    const wng::Node& require_node(const wng::Graph& graph, wng::NodeId id)
    {
        const wng::Node* node = graph.find_node(id);
        assert(node != nullptr);
        return *node;
    }

    bool contains_node(const wng::Graph& graph, wng::NodeId id)
    {
        return graph.find_node(id) != nullptr;
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
}

int main()
{
    {
        // Empty mixed history reports that no operation can be moved. This keeps
        // future editor command routing from synthesizing fake no-op entries.
        wng::Graph graph;
        wng::GraphHistory history;

        assert(!history.can_undo());
        assert(!history.can_redo());
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);

        const wng::GraphHistoryOperationResult undo =
            wng::undo_last_graph_history(graph, history);
        const wng::GraphHistoryOperationResult redo =
            wng::redo_last_graph_history(graph, history);

        assert(!undo.success());
        assert(undo.result == wng::Result::NotFound);
        assert(!redo.success());
        assert(redo.result == wng::Result::NotFound);
    }

    {
        // Empty graph command batches are rejected because they would create
        // undoable history steps that perform no graph work.
        wng::GraphHistory history;
        const wng::GraphCommandBatch batch;

        assert(history.record_graph_command_batch(batch) == wng::Result::InvalidArgument);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Failed graph command batches are diagnostics only. They are rejected so
        // partial command data cannot enter the user-level history timeline.
        wng::Graph graph;
        wng::GraphHistory history;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        wng::GraphCommandBatch batch = make_batch({ create.record });
        batch.result = wng::Result::InvalidArgument;

        assert(history.record_graph_command_batch(batch) == wng::Result::InvalidArgument);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Failed single records are also rejected before normalization into a
        // batch, preserving the same safety rule for both recording APIs.
        wng::Graph graph;
        wng::GraphHistory history;
        const wng::GraphCommandResult failed =
            wng::command_add_port(
                graph,
                wng::NodeId { 999 },
                make_port_desc(wng::PortKind::Input, "in"));

        assert(failed.result != wng::Result::Ok);
        assert(history.record_graph_command(failed.record) == wng::Result::InvalidArgument);
        assert(history.undo_count() == 0U);
    }

    {
        // Empty migration records have no before/after snapshots to restore, so
        // they cannot be valid mixed history entries.
        wng::GraphHistory history;
        const wng::SchemaMigrationApplyCommandRecord record;

        assert(record.empty());
        assert(history.record_schema_migration_apply_command(record) ==
            wng::Result::InvalidArgument);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Non-applied migration command records are rejected even when the command
        // wrapper captured diagnostic state. Only completed graph transforms are
        // safe undo/redo units.
        wng::Graph graph;
        create_node_direct(graph, "legacy.node");

        const wng::GraphSchema source = make_schema({ node_definition("legacy.node") });
        const wng::GraphSchema target = make_schema({ node_definition("modern.node") });
        const wng::SchemaMigrationPolicy empty_policy;
        const wng::SchemaMigrationApplyCommandResult failed =
            wng::command_apply_schema_migration(graph, source, target, empty_policy);

        wng::GraphHistory history;
        assert(!failed.success());
        assert(history.record_schema_migration_apply_command(failed.record) ==
            wng::Result::InvalidArgument);
        assert(history.undo_count() == 0U);
    }

    {
        // A normal graph command recorded in GraphHistory delegates undo/redo to
        // the existing graph command batch primitives and restores stable IDs.
        wng::Graph graph;
        wng::GraphHistory history;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        const wng::NodeId node = create.record.node;

        assert(history.record_graph_command(create.record) == wng::Result::Ok);
        assert(history.can_undo());
        assert(history.undo_count() == 1U);

        assert(wng::undo_last_graph_history(graph, history).success());
        assert(!contains_node(graph, node));
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);

        assert(wng::redo_last_graph_history(graph, history).success());
        assert(contains_node(graph, node));
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
    }

    {
        // A schema migration command recorded in GraphHistory delegates undo/redo
        // to snapshot restoration instead of forcing migration effects into
        // GraphCommandRecord's object create/remove model.
        wng::Graph graph;
        const wng::NodeId node = create_node_direct(graph, "legacy.node");
        const wng::SchemaMigrationApplyCommandResult command =
            apply_rename_command(graph, "legacy.node", "modern.node");
        assert(command.success());
        assert(require_node(graph, node).type == "modern.node");

        wng::GraphHistory history;
        assert(history.record_schema_migration_apply_command(command.record) ==
            wng::Result::Ok);

        assert(wng::undo_last_graph_history(graph, history).success());
        assert(require_node(graph, node).type == "legacy.node");
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);

        assert(wng::redo_last_graph_history(graph, history).success());
        assert(require_node(graph, node).type == "modern.node");
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
    }

    {
        // Verifies unified history preserves user-level chronology across normal
        // graph commands and schema migration snapshot commands. Separate histories
        // cannot express the required undo order C, then B, then A.
        wng::Graph graph;
        wng::GraphHistory history;

        const wng::GraphCommandResult create_a =
            wng::command_create_node(graph, make_node_desc("A", "legacy.node"));
        const wng::NodeId a = create_a.record.node;
        assert(history.record_graph_command(create_a.record) == wng::Result::Ok);

        const wng::SchemaMigrationApplyCommandResult migrate =
            apply_rename_command(graph, "legacy.node", "modern.node");
        assert(migrate.success());
        assert(history.record_schema_migration_apply_command(migrate.record) ==
            wng::Result::Ok);
        assert(require_node(graph, a).type == "modern.node");

        const wng::GraphCommandResult create_c =
            wng::command_create_node(graph, make_node_desc("C", "other.node"));
        const wng::NodeId c = create_c.record.node;
        assert(history.record_graph_command(create_c.record) == wng::Result::Ok);
        assert(history.undo_count() == 3U);

        assert(wng::undo_last_graph_history(graph, history).success());
        assert(!contains_node(graph, c));
        assert(contains_node(graph, a));
        assert(require_node(graph, a).type == "modern.node");

        assert(wng::undo_last_graph_history(graph, history).success());
        assert(contains_node(graph, a));
        assert(require_node(graph, a).type == "legacy.node");
        assert(!contains_node(graph, c));

        assert(wng::undo_last_graph_history(graph, history).success());
        assert(!contains_node(graph, a));
        assert(!contains_node(graph, c));
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 3U);

        assert(wng::redo_last_graph_history(graph, history).success());
        assert(contains_node(graph, a));
        assert(require_node(graph, a).type == "legacy.node");
        assert(!contains_node(graph, c));

        assert(wng::redo_last_graph_history(graph, history).success());
        assert(require_node(graph, a).type == "modern.node");
        assert(!contains_node(graph, c));

        assert(wng::redo_last_graph_history(graph, history).success());
        assert(contains_node(graph, a));
        assert(contains_node(graph, c));
        assert(history.undo_count() == 3U);
        assert(history.redo_count() == 0U);
    }

    {
        // Recording a new mixed entry after undo invalidates the redo branch. This
        // prevents replaying operations that no longer follow the current graph
        // state chronology.
        wng::Graph graph;
        wng::GraphHistory history;
        const wng::GraphCommandResult create_a =
            wng::command_create_node(graph, make_node_desc("A"));
        const wng::GraphCommandResult create_b =
            wng::command_create_node(graph, make_node_desc("B"));

        assert(history.record_graph_command(create_a.record) == wng::Result::Ok);
        assert(history.record_graph_command(create_b.record) == wng::Result::Ok);
        assert(wng::undo_last_graph_history(graph, history).success());
        assert(history.redo_count() == 1U);

        const wng::GraphCommandResult create_c =
            wng::command_create_node(graph, make_node_desc("C"));
        assert(history.record_graph_command(create_c.record) == wng::Result::Ok);

        assert(history.undo_count() == 2U);
        assert(history.redo_count() == 0U);
        assert(!history.can_redo());
    }

    {
        // Failed graph-command undo must not move the mixed history entry. The
        // graph is intentionally mutated out of band to make the recorded create
        // command impossible to undo through normal command primitives.
        wng::Graph graph;
        wng::GraphHistory history;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(history.record_graph_command(create.record) == wng::Result::Ok);

        wng::GraphMutationSummary summary;
        assert(graph.destroy_node(create.record.node, &summary) == wng::Result::Ok);

        const wng::GraphHistoryOperationResult undo =
            wng::undo_last_graph_history(graph, history);

        assert(!undo.success());
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
    }

    {
        // Existing specialized histories remain usable independently. GraphHistory
        // does not replace their focused stacks; it only adds a higher-level mixed
        // owner for callers that need one chronological timeline.
        wng::Graph command_graph;
        wng::GraphCommandHistory command_history;
        const wng::GraphCommandResult create =
            wng::command_create_node(command_graph, make_node_desc("A"));
        assert(command_history.record(create.record) == wng::Result::Ok);
        assert(wng::undo_last(command_graph, command_history).success());
        assert(wng::redo_last(command_graph, command_history).success());

        wng::Graph migration_graph;
        const wng::NodeId node = create_node_direct(migration_graph, "legacy.node");
        const wng::SchemaMigrationApplyCommandResult command =
            apply_rename_command(migration_graph, "legacy.node", "modern.node");
        wng::SchemaMigrationApplyCommandHistory migration_history;
        assert(migration_history.record(command.record) == wng::Result::Ok);
        assert(wng::undo_last_schema_migration_apply_command(
            migration_graph,
            migration_history).success());
        assert(require_node(migration_graph, node).type == "legacy.node");
    }

    return 0;
}
