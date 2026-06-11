// Exercises minimal history stack ownership for graph command records.
// These tests intentionally keep editor state, command execution, and WPL
// integration out of the history layer.

#include <cassert>
#include <vector>

#include <wng/graph_command_history.hpp>
#include <wng/graph_restore.hpp>
#include <wng/graph_validation.hpp>

namespace
{
    wng::NodeDesc make_node_desc(const char* title = "Node")
    {
        wng::NodeDesc desc;
        desc.type = "history.node";
        desc.title = title;
        desc.position = wng::Vec2 { 1.0f, 2.0f };
        desc.size = wng::Vec2 { 100.0f, 50.0f };
        return desc;
    }

    wng::PortDesc make_port_desc(
        wng::PortKind kind,
        const char* name,
        const char* type = "number")
    {
        wng::PortDesc desc;
        desc.kind = kind;
        desc.name = name;
        desc.type = type;
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

    bool contains_node(const wng::Graph& graph, wng::NodeId id)
    {
        return graph.find_node(id) != nullptr;
    }

    bool contains_port(const wng::Graph& graph, wng::PortId id)
    {
        return graph.find_port(id) != nullptr;
    }

    bool contains_link(const wng::Graph& graph, wng::LinkId id)
    {
        return graph.find_link(id) != nullptr;
    }

    bool node_has_port(const wng::Node& node, wng::PortId port)
    {
        for (wng::PortId input : node.inputs) {
            if (input == port) {
                return true;
            }
        }

        for (wng::PortId output : node.outputs) {
            if (output == port) {
                return true;
            }
        }

        return false;
    }
}

int main()
{
    {
        // New histories start empty so editor code can query capabilities without
        // special setup or sentinel entries.
        wng::GraphCommandHistory history;

        assert(!history.can_undo());
        assert(!history.can_redo());
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Successful command records become undoable history entries. Recording
        // does not execute a command; it stores the already captured graph effect.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));

        assert(history.record(create.record) == wng::Result::Ok);
        assert(history.can_undo());
        assert(!history.can_redo());
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
    }

    {
        // Failed command records are diagnostics only. They are rejected as
        // history entries because there is no reliable graph effect to undo.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        const wng::GraphCommandResult failed =
            wng::command_add_port(
                graph,
                wng::NodeId { 999 },
                make_port_desc(wng::PortKind::Input, "in"));

        assert(failed.result != wng::Result::Ok);
        assert(history.record(failed.record) == wng::Result::InvalidArgument);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Undo moves a history entry to redo only after the graph undo primitive
        // succeeds, preserving stack consistency with graph state.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(history.record(create.record) == wng::Result::Ok);

        const wng::GraphHistoryResult undo = wng::undo_last(graph, history);

        assert(undo.result == wng::Result::Ok);
        assert(!contains_node(graph, create.record.node));
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);
        assert(!history.can_undo());
        assert(history.can_redo());
    }

    {
        // Redo moves the entry back to undo after reapplying the recorded graph
        // effect with stable IDs restored from the command record.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        const wng::NodeId created_node = create.record.node;
        assert(history.record(create.record) == wng::Result::Ok);
        assert(wng::undo_last(graph, history).success());

        const wng::GraphHistoryResult redo = wng::redo_last(graph, history);

        assert(redo.result == wng::Result::Ok);
        assert(contains_node(graph, created_node));
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
        assert(history.can_undo());
        assert(!history.can_redo());
    }

    {
        // Empty undo histories report NotFound and leave both graph and stacks
        // unchanged; no no-op history entries are synthesized.
        wng::Graph graph;
        wng::GraphCommandHistory history;

        const wng::GraphHistoryResult result = wng::undo_last(graph, history);

        assert(result.result == wng::Result::NotFound);
        assert(graph.nodes().empty());
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Empty redo histories mirror undo behavior and never mutate graph state.
        wng::Graph graph;
        wng::GraphCommandHistory history;

        const wng::GraphHistoryResult result = wng::redo_last(graph, history);

        assert(result.result == wng::Result::NotFound);
        assert(graph.nodes().empty());
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Verifies redo-branch invalidation. Once a user records a new command
        // after undoing, the previous redo stack must be discarded.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        const wng::GraphCommandResult create_a =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(history.record(create_a.record) == wng::Result::Ok);
        assert(wng::undo_last(graph, history).success());
        assert(history.redo_count() == 1U);

        const wng::GraphCommandResult create_b =
            wng::command_create_node(graph, make_node_desc("B"));
        assert(history.record(create_b.record) == wng::Result::Ok);

        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
        assert(wng::redo_last(graph, history).result == wng::Result::NotFound);
    }

    {
        // Batches are stored as one user-level history step. Undo removes all
        // created objects together, and redo restores them with original IDs.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        const wng::GraphCommandResult create_a =
            wng::command_create_node(graph, make_node_desc("A"));
        const wng::GraphCommandResult create_b =
            wng::command_create_node(graph, make_node_desc("B"));
        const wng::GraphCommandResult add_a_out =
            wng::command_add_port(
                graph,
                create_a.record.node,
                make_port_desc(wng::PortKind::Output, "out"));
        const wng::GraphCommandResult add_b_in =
            wng::command_add_port(
                graph,
                create_b.record.node,
                make_port_desc(wng::PortKind::Input, "in"));
        const wng::GraphCommandResult create_link =
            wng::command_create_link(
                graph,
                add_a_out.record.port,
                add_b_in.record.port);

        const wng::NodeId a = create_a.record.node;
        const wng::NodeId b = create_b.record.node;
        const wng::PortId a_out = add_a_out.record.port;
        const wng::PortId b_in = add_b_in.record.port;
        const wng::LinkId link = create_link.record.link;

        const wng::GraphCommandBatch batch = make_batch({
            create_a.record,
            create_b.record,
            add_a_out.record,
            add_b_in.record,
            create_link.record
        });

        assert(history.record_batch(batch) == wng::Result::Ok);
        assert(history.undo_count() == 1U);

        assert(wng::undo_last(graph, history).success());
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
        assert(graph.links().empty());
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);

        assert(wng::redo_last(graph, history).success());
        assert(contains_node(graph, a));
        assert(contains_node(graph, b));
        assert(contains_port(graph, a_out));
        assert(contains_port(graph, b_in));
        assert(contains_link(graph, link));
        assert(wng::validate_graph(graph).valid());
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
    }

    {
        // Failed batches are not safe undo units. Rejecting them keeps partial
        // diagnostic data out of the history stack.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        wng::GraphCommandBatch batch = make_batch({ create.record });
        batch.result = wng::Result::InvalidArgument;

        assert(history.record_batch(batch) == wng::Result::InvalidArgument);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Empty successful batches are rejected to avoid confusing can_undo()
        // behavior where an undoable entry would perform no graph work.
        wng::GraphCommandHistory history;
        wng::GraphCommandBatch batch;
        batch.result = wng::Result::Ok;

        assert(history.record_batch(batch) == wng::Result::InvalidArgument);
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Failed undo application must not move the history entry. This protects
        // stack atomicity when the graph no longer matches the recorded effect.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(history.record(create.record) == wng::Result::Ok);

        wng::GraphMutationSummary summary;
        assert(graph.destroy_node(create.record.node, &summary) == wng::Result::Ok);
        const std::size_t node_count_before = graph.nodes().size();

        const wng::GraphHistoryResult undo = wng::undo_last(graph, history);

        assert(undo.result != wng::Result::Ok);
        assert(graph.nodes().size() == node_count_before);
        assert(history.undo_count() == 1U);
        assert(history.redo_count() == 0U);
    }

    {
        // Failed redo application must not move the history entry. A collision is
        // created manually to verify redo failure leaves stacks and graph intact.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(history.record(create.record) == wng::Result::Ok);
        assert(wng::undo_last(graph, history).success());

        wng::GraphObjectSnapshot collision;
        collision.nodes = create.record.created_nodes;
        assert(wng::restore_graph_objects(graph, collision).success());
        const std::size_t node_count_before = graph.nodes().size();

        const wng::GraphHistoryResult redo = wng::redo_last(graph, history);

        assert(redo.result != wng::Result::Ok);
        assert(graph.nodes().size() == node_count_before);
        assert(contains_node(graph, create.record.node));
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 1U);
    }

    {
        // Clear helpers are stack-only operations. They do not mutate Graph and
        // let future editor code explicitly discard history when needed.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(history.record(create.record) == wng::Result::Ok);
        assert(wng::undo_last(graph, history).success());
        assert(history.redo_count() == 1U);

        history.clear_redo();
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);

        const wng::GraphCommandResult create_b =
            wng::command_create_node(graph, make_node_desc("B"));
        assert(history.record(create_b.record) == wng::Result::Ok);
        history.clear();
        assert(history.undo_count() == 0U);
        assert(history.redo_count() == 0U);
    }

    {
        // Undoing an add-port history entry removes the port from its owning node;
        // redoing it restores both the stable PortId and the node port list.
        wng::Graph graph;
        wng::GraphCommandHistory history;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        const wng::GraphCommandResult add_port =
            wng::command_add_port(
                graph,
                create.record.node,
                make_port_desc(wng::PortKind::Input, "in"));
        assert(history.record(add_port.record) == wng::Result::Ok);

        assert(wng::undo_last(graph, history).success());
        assert(!contains_port(graph, add_port.record.port));
        assert(!node_has_port(*graph.find_node(create.record.node), add_port.record.port));

        assert(wng::redo_last(graph, history).success());
        assert(contains_port(graph, add_port.record.port));
        assert(node_has_port(*graph.find_node(create.record.node), add_port.record.port));
    }

    return 0;
}
