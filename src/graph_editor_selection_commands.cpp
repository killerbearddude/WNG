// Implements WPL-free selection-driven graph deletion commands.
// This file uses GraphSession as the working graph/history owner and
// GraphEditorState as stable-ID editor state; it does not perform hit testing,
// rendering, widget work, platform input handling, or WPL integration.

#include <wng/graph_editor_selection_commands.hpp>

#include <algorithm>
#include <cstdint>
#include <new>
#include <vector>

#include <wng/graph_command.hpp>
#include <wng/graph_editor_state_cleanup.hpp>

namespace
{
    template <typename Id>
    std::uint32_t id_value(Id id)
    {
        return id.value;
    }

    template <typename Id>
    void normalize_ids(std::vector<Id>& ids)
    {
        ids.erase(
            std::remove_if(
                ids.begin(),
                ids.end(),
                [](Id id) { return id_value(id) == 0; }),
            ids.end());
        std::sort(
            ids.begin(),
            ids.end(),
            [](Id lhs, Id rhs) { return id_value(lhs) < id_value(rhs); });
        ids.erase(
            std::unique(ids.begin(), ids.end()),
            ids.end());
    }

    void capture_initial_availability(
        wng::GraphEditorSelectionCommandResult& result,
        const wng::GraphSession& session,
        const wng::GraphEditorState& editor_state)
    {
        result.before = wng::graph_editor_selection_command_availability(
            session.graph(),
            editor_state);
        result.after = result.before;
    }

    void capture_final_availability(
        wng::GraphEditorSelectionCommandResult& result,
        const wng::GraphSession& session,
        const wng::GraphEditorState& editor_state)
    {
        result.after = wng::graph_editor_selection_command_availability(
            session.graph(),
            editor_state);
    }

    template <typename Id>
    bool snapshot_ids(
        const std::vector<Id>& selected,
        std::vector<Id>& out_ids,
        wng::GraphEditorSelectionCommandResult& result)
    {
        try {
            out_ids = selected;
            normalize_ids(out_ids);
            return true;
        } catch (const std::bad_alloc&) {
            result.result = wng::Result::AllocationFailure;
            return false;
        }
    }

    bool append_successful_command(
        wng::GraphEditorSelectionCommandResult& result,
        wng::GraphSession& session,
        const wng::GraphCommandResult& command)
    {
        const wng::Result append_result = result.transaction.append(command);
        if (append_result == wng::Result::Ok) {
            return true;
        }

        // The graph command already mutated the session graph, but the command
        // record could not be retained. Clear the partial transaction so callers
        // cannot accidentally commit an incomplete history entry.
        result.transaction.clear();
        result.result = append_result;
        session.mark_modified();
        return false;
    }

    void commit_if_needed(
        wng::GraphEditorSelectionCommandResult& result,
        wng::GraphSession& session)
    {
        if (result.transaction.empty()) {
            return;
        }

        result.commit_result = session.commit_transaction(result.transaction);
        if (result.result == wng::Result::Ok && !result.commit_result.success()) {
            result.result = result.commit_result.result;
        }
    }

    bool keep_deleting(const wng::GraphEditorSelectionCommandResult& result)
    {
        return result.result == wng::Result::Ok;
    }

    void delete_links_into(
        wng::GraphSession& session,
        wng::GraphEditorState& editor_state,
        const std::vector<wng::LinkId>& links,
        wng::GraphEditorSelectionCommandResult& result)
    {
        for (wng::LinkId link : links) {
            if (!keep_deleting(result)) {
                return;
            }

            const wng::GraphCommandResult command =
                wng::command_destroy_link(session.graph(), link);
            if (command.success()) {
                wng::apply_editor_state_cleanup(editor_state, command);
                ++result.deleted_links;
                if (!append_successful_command(result, session, command)) {
                    return;
                }
                continue;
            }

            if (command.result == wng::Result::NotFound ||
                command.result == wng::Result::InvalidArgument) {
                editor_state.deselect_link(link);
                continue;
            }

            result.result = command.result;
        }
    }

    void delete_ports_into(
        wng::GraphSession& session,
        wng::GraphEditorState& editor_state,
        const std::vector<wng::PortId>& ports,
        wng::GraphEditorSelectionCommandResult& result)
    {
        for (wng::PortId port : ports) {
            if (!keep_deleting(result)) {
                return;
            }

            const wng::GraphCommandResult command =
                wng::command_remove_port(session.graph(), port);
            if (command.success()) {
                wng::apply_editor_state_cleanup(editor_state, command);
                ++result.deleted_ports;
                if (!append_successful_command(result, session, command)) {
                    return;
                }
                continue;
            }

            if (command.result == wng::Result::NotFound ||
                command.result == wng::Result::InvalidArgument) {
                editor_state.deselect_port(port);
                continue;
            }

            result.result = command.result;
        }
    }

    void delete_nodes_into(
        wng::GraphSession& session,
        wng::GraphEditorState& editor_state,
        const std::vector<wng::NodeId>& nodes,
        wng::GraphEditorSelectionCommandResult& result)
    {
        for (wng::NodeId node : nodes) {
            if (!keep_deleting(result)) {
                return;
            }

            const wng::GraphCommandResult command =
                wng::command_destroy_node(session.graph(), node);
            if (command.success()) {
                wng::apply_editor_state_cleanup(editor_state, command);
                ++result.deleted_nodes;
                if (!append_successful_command(result, session, command)) {
                    return;
                }
                continue;
            }

            if (command.result == wng::Result::NotFound ||
                command.result == wng::Result::InvalidArgument) {
                editor_state.deselect_node(node);
                continue;
            }

            result.result = command.result;
        }
    }
}

namespace wng
{
    bool GraphEditorSelectionCommandResult::success() const
    {
        return result == Result::Ok && commit_result.success();
    }

    std::size_t GraphEditorSelectionCommandResult::deleted_count() const
    {
        return deleted_links + deleted_ports + deleted_nodes;
    }

    GraphEditorSelectionCommandResult delete_selected_links(
        GraphSession& session,
        GraphEditorState& editor_state)
    {
        GraphEditorSelectionCommandResult result;
        capture_initial_availability(result, session, editor_state);

        std::vector<LinkId> links;
        if (!snapshot_ids(editor_state.selected_links(), links, result)) {
            capture_final_availability(result, session, editor_state);
            return result;
        }

        delete_links_into(session, editor_state, links, result);
        commit_if_needed(result, session);
        capture_final_availability(result, session, editor_state);
        return result;
    }

    GraphEditorSelectionCommandResult delete_selected_ports(
        GraphSession& session,
        GraphEditorState& editor_state)
    {
        GraphEditorSelectionCommandResult result;
        capture_initial_availability(result, session, editor_state);

        std::vector<PortId> ports;
        if (!snapshot_ids(editor_state.selected_ports(), ports, result)) {
            capture_final_availability(result, session, editor_state);
            return result;
        }

        delete_ports_into(session, editor_state, ports, result);
        commit_if_needed(result, session);
        capture_final_availability(result, session, editor_state);
        return result;
    }

    GraphEditorSelectionCommandResult delete_selected_nodes(
        GraphSession& session,
        GraphEditorState& editor_state)
    {
        GraphEditorSelectionCommandResult result;
        capture_initial_availability(result, session, editor_state);

        std::vector<NodeId> nodes;
        if (!snapshot_ids(editor_state.selected_nodes(), nodes, result)) {
            capture_final_availability(result, session, editor_state);
            return result;
        }

        delete_nodes_into(session, editor_state, nodes, result);
        commit_if_needed(result, session);
        capture_final_availability(result, session, editor_state);
        return result;
    }

    GraphEditorSelectionCommandResult delete_selected_graph_objects(
        GraphSession& session,
        GraphEditorState& editor_state)
    {
        GraphEditorSelectionCommandResult result;
        capture_initial_availability(result, session, editor_state);

        std::vector<LinkId> links;
        std::vector<PortId> ports;
        std::vector<NodeId> nodes;

        // Snapshot the entire selection before any mutation, because deleting one
        // object can clean related selection state through mutation summaries.
        if (!snapshot_ids(editor_state.selected_links(), links, result) ||
            !snapshot_ids(editor_state.selected_ports(), ports, result) ||
            !snapshot_ids(editor_state.selected_nodes(), nodes, result)) {
            capture_final_availability(result, session, editor_state);
            return result;
        }

        // Record order is links -> ports -> nodes. Batch undo reverses that order,
        // restoring owner nodes before ports and ports before links.
        delete_links_into(session, editor_state, links, result);
        delete_ports_into(session, editor_state, ports, result);
        delete_nodes_into(session, editor_state, nodes, result);
        commit_if_needed(result, session);
        capture_final_availability(result, session, editor_state);
        return result;
    }
}
