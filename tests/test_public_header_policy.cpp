#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <wng/graph_editor_state.hpp>

namespace
{
    std::string read_file(const std::string& path)
    {
        std::ifstream file(path);
        assert(file.is_open());

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    bool contains(const std::string& text, const std::string& needle)
    {
        return text.find(needle) != std::string::npos;
    }

    void require_absent(const std::string& text, const std::string& needle)
    {
        assert(!contains(text, needle));
    }

    void assert_editor_state_availability()
    {
        wng::GraphEditorState editor_state;
        wng::GraphEditorCommandAvailability availability =
            wng::graph_editor_command_availability(editor_state);

        assert(!availability.clear_selection);
        assert(!availability.remove_selection);
        assert(!availability.clear_hover);
        assert(!availability.cancel_pending_link);
        assert(!availability.complete_pending_link);
        assert(!availability.any_available());

        const wng::NodeId node { 11U };
        const wng::PortId source_port { 21U };
        const wng::PortId target_port { 22U };

        assert(editor_state.select_node(node) == wng::Result::Ok);
        availability = wng::graph_editor_command_availability(editor_state);
        assert(availability.clear_selection);
        assert(availability.remove_selection);
        assert(!availability.clear_hover);
        assert(!availability.cancel_pending_link);
        assert(!availability.complete_pending_link);
        assert(availability.any_available());

        assert(editor_state.set_hovered_port(target_port) == wng::Result::Ok);
        availability = wng::graph_editor_command_availability(editor_state);
        assert(availability.clear_selection);
        assert(availability.remove_selection);
        assert(availability.clear_hover);
        assert(!availability.cancel_pending_link);
        assert(!availability.complete_pending_link);
        assert(availability.any_available());

        assert(editor_state.begin_pending_link(source_port) == wng::Result::Ok);
        availability = wng::graph_editor_command_availability(editor_state);
        assert(availability.cancel_pending_link);
        assert(!availability.complete_pending_link);
        assert(availability.any_available());

        assert(editor_state.set_pending_link_target(source_port) == wng::Result::Ok);
        availability = wng::graph_editor_command_availability(editor_state);
        assert(availability.cancel_pending_link);
        assert(!availability.complete_pending_link);

        assert(editor_state.set_pending_link_target(target_port) == wng::Result::Ok);
        availability = wng::graph_editor_command_availability(editor_state);
        assert(availability.cancel_pending_link);
        assert(availability.complete_pending_link);
        assert(availability.any_available());

        editor_state.clear_selection();
        editor_state.clear_hovered();
        editor_state.clear_pending_link();
        availability = wng::graph_editor_command_availability(editor_state);
        assert(!availability.clear_selection);
        assert(!availability.remove_selection);
        assert(!availability.clear_hover);
        assert(!availability.cancel_pending_link);
        assert(!availability.complete_pending_link);
        assert(!availability.any_available());
    }

    void assert_editor_state_set_commands()
    {
        wng::GraphEditorState editor_state;
        const wng::NodeId node { 11U };
        const wng::PortId source_port { 21U };
        const wng::PortId target_port { 22U };
        const wng::LinkId link { 31U };

        wng::GraphEditorStateCommandResult command =
            wng::select_graph_editor_node(editor_state, wng::NodeId {});
        assert(command.result == wng::Result::InvalidArgument);
        assert(!command.success());
        assert(!command.changed);
        assert(!command.before.clear_selection);
        assert(!command.after.clear_selection);

        command = wng::select_graph_editor_node(editor_state, node);
        assert(command.success());
        assert(command.changed);
        assert(!command.before.clear_selection);
        assert(command.after.clear_selection);
        assert(command.after.remove_selection);
        assert(editor_state.node_selected(node));

        command = wng::select_graph_editor_node(editor_state, node);
        assert(command.result == wng::Result::AlreadyExists);
        assert(!command.success());
        assert(!command.changed);
        assert(command.before.clear_selection);
        assert(command.after.clear_selection);

        command = wng::select_graph_editor_port(editor_state, source_port);
        assert(command.success());
        assert(command.changed);
        assert(command.before.clear_selection);
        assert(command.after.clear_selection);
        assert(editor_state.port_selected(source_port));

        command = wng::select_graph_editor_link(editor_state, link);
        assert(command.success());
        assert(command.changed);
        assert(editor_state.link_selected(link));

        command = wng::set_graph_editor_hovered_node(editor_state, wng::NodeId {});
        assert(command.result == wng::Result::InvalidArgument);
        assert(!command.success());
        assert(!command.changed);
        assert(!command.before.clear_hover);
        assert(!command.after.clear_hover);

        command = wng::set_graph_editor_hovered_node(editor_state, node);
        assert(command.success());
        assert(command.changed);
        assert(!command.before.clear_hover);
        assert(command.after.clear_hover);
        assert(editor_state.hovered().kind == wng::GraphEditorElementKind::Node);
        assert(editor_state.hovered().node == node);

        command = wng::set_graph_editor_hovered_node(editor_state, node);
        assert(command.success());
        assert(!command.changed);
        assert(command.before.clear_hover);
        assert(command.after.clear_hover);

        command = wng::set_graph_editor_hovered_port(editor_state, source_port);
        assert(command.success());
        assert(command.changed);
        assert(editor_state.hovered().kind == wng::GraphEditorElementKind::Port);
        assert(editor_state.hovered().port == source_port);

        command = wng::set_graph_editor_hovered_link(editor_state, link);
        assert(command.success());
        assert(command.changed);
        assert(editor_state.hovered().kind == wng::GraphEditorElementKind::Link);
        assert(editor_state.hovered().link == link);

        command = wng::begin_graph_editor_pending_link(editor_state, wng::PortId {});
        assert(command.result == wng::Result::InvalidArgument);
        assert(!command.success());
        assert(!command.changed);
        assert(!command.before.cancel_pending_link);
        assert(!command.after.cancel_pending_link);

        wng::GraphEditorState inactive_pending_state;
        command = wng::set_graph_editor_pending_link_target(
            inactive_pending_state,
            target_port);
        assert(command.result == wng::Result::InvalidArgument);
        assert(!command.success());
        assert(!command.changed);
        assert(!command.before.cancel_pending_link);
        assert(!command.after.cancel_pending_link);

        command = wng::begin_graph_editor_pending_link(editor_state, source_port);
        assert(command.success());
        assert(command.changed);
        assert(!command.before.cancel_pending_link);
        assert(command.after.cancel_pending_link);
        assert(!command.after.complete_pending_link);

        command = wng::begin_graph_editor_pending_link(editor_state, source_port);
        assert(command.success());
        assert(!command.changed);
        assert(command.before.cancel_pending_link);
        assert(command.after.cancel_pending_link);

        command = wng::set_graph_editor_pending_link_target(editor_state, source_port);
        assert(command.success());
        assert(command.changed);
        assert(command.before.cancel_pending_link);
        assert(command.after.cancel_pending_link);
        assert(!command.after.complete_pending_link);

        command = wng::set_graph_editor_pending_link_target(editor_state, target_port);
        assert(command.success());
        assert(command.changed);
        assert(command.before.cancel_pending_link);
        assert(!command.before.complete_pending_link);
        assert(command.after.cancel_pending_link);
        assert(command.after.complete_pending_link);

        command = wng::set_graph_editor_pending_link_target(editor_state, target_port);
        assert(command.success());
        assert(!command.changed);
        assert(command.before.complete_pending_link);
        assert(command.after.complete_pending_link);
    }

    void assert_editor_state_clear_commands()
    {
        wng::GraphEditorState editor_state;
        wng::GraphEditorStateCommandResult command =
            wng::clear_graph_editor_selection(editor_state);
        assert(command.result == wng::Result::InvalidArgument);
        assert(!command.success());
        assert(!command.changed);
        assert(!command.before.clear_selection);
        assert(!command.after.clear_selection);

        const wng::NodeId node { 11U };
        const wng::PortId source_port { 21U };
        const wng::PortId target_port { 22U };

        assert(editor_state.select_node(node) == wng::Result::Ok);
        command = wng::clear_graph_editor_selection(editor_state);
        assert(command.success());
        assert(command.changed);
        assert(command.before.clear_selection);
        assert(command.before.remove_selection);
        assert(!command.after.clear_selection);
        assert(!command.after.remove_selection);
        assert(!wng::graph_editor_has_selection(editor_state));

        command = wng::clear_graph_editor_hover(editor_state);
        assert(command.result == wng::Result::InvalidArgument);
        assert(!command.changed);
        assert(!command.before.clear_hover);
        assert(!command.after.clear_hover);

        assert(editor_state.set_hovered_port(target_port) == wng::Result::Ok);
        command = wng::clear_graph_editor_hover(editor_state);
        assert(command.success());
        assert(command.changed);
        assert(command.before.clear_hover);
        assert(!command.after.clear_hover);
        assert(!wng::graph_editor_has_hover(editor_state));

        command = wng::cancel_graph_editor_pending_link(editor_state);
        assert(command.result == wng::Result::InvalidArgument);
        assert(!command.changed);
        assert(!command.before.cancel_pending_link);
        assert(!command.after.cancel_pending_link);

        assert(editor_state.begin_pending_link(source_port) == wng::Result::Ok);
        assert(editor_state.set_pending_link_target(target_port) == wng::Result::Ok);
        command = wng::cancel_graph_editor_pending_link(editor_state);
        assert(command.success());
        assert(command.changed);
        assert(command.before.cancel_pending_link);
        assert(command.before.complete_pending_link);
        assert(!command.after.cancel_pending_link);
        assert(!command.after.complete_pending_link);
        assert(!wng::graph_editor_has_active_pending_link(editor_state));
    }

    void assert_editor_state_command_status()
    {
        wng::GraphEditorState editor_state;
        const wng::GraphEditorCommandId invalid_command =
            static_cast<wng::GraphEditorCommandId>(99);

        wng::GraphEditorCommandStatus status = wng::graph_editor_command_status(
            editor_state,
            wng::GraphEditorCommandId::ClearSelection);
        assert(!status.available);
        assert(status.unavailable_reason ==
            wng::GraphEditorCommandUnavailableReason::NoSelection);
        assert(!wng::graph_editor_command_available(
            editor_state,
            wng::GraphEditorCommandId::ClearSelection));

        status = wng::graph_editor_command_status(
            editor_state,
            wng::GraphEditorCommandId::ClearHover);
        assert(!status.available);
        assert(status.unavailable_reason ==
            wng::GraphEditorCommandUnavailableReason::NoHover);

        status = wng::graph_editor_command_status(
            editor_state,
            wng::GraphEditorCommandId::CancelPendingLink);
        assert(!status.available);
        assert(status.unavailable_reason ==
            wng::GraphEditorCommandUnavailableReason::NoActivePendingLink);

        status = wng::graph_editor_command_status(editor_state, invalid_command);
        assert(!status.available);
        assert(status.unavailable_reason ==
            wng::GraphEditorCommandUnavailableReason::InvalidCommand);

        const wng::NodeId node { 11U };
        const wng::PortId source_port { 21U };
        const wng::PortId target_port { 22U };

        assert(editor_state.select_node(node) == wng::Result::Ok);
        wng::GraphEditorCommandAvailability availability =
            wng::graph_editor_command_availability(editor_state);
        status = wng::graph_editor_command_status(
            availability,
            wng::GraphEditorCommandId::ClearSelection);
        assert(status.available);
        assert(status.unavailable_reason ==
            wng::GraphEditorCommandUnavailableReason::None);
        assert(wng::graph_editor_command_available(
            availability,
            wng::GraphEditorCommandId::ClearSelection));

        assert(editor_state.set_hovered_port(target_port) == wng::Result::Ok);
        status = wng::graph_editor_command_status(
            editor_state,
            wng::GraphEditorCommandId::ClearHover);
        assert(status.available);
        assert(status.unavailable_reason ==
            wng::GraphEditorCommandUnavailableReason::None);

        assert(editor_state.begin_pending_link(source_port) == wng::Result::Ok);
        status = wng::graph_editor_command_status(
            editor_state,
            wng::GraphEditorCommandId::CancelPendingLink);
        assert(status.available);
        assert(status.unavailable_reason ==
            wng::GraphEditorCommandUnavailableReason::None);
    }

    void assert_editor_state_command_dispatch()
    {
        wng::GraphEditorState editor_state;
        const wng::GraphEditorCommandId invalid_command =
            static_cast<wng::GraphEditorCommandId>(99);

        assert(!wng::graph_editor_command_available(
            editor_state,
            wng::GraphEditorCommandId::ClearSelection));
        assert(!wng::graph_editor_command_available(editor_state, invalid_command));

        wng::GraphEditorStateCommandResult command =
            wng::run_graph_editor_state_command(
                editor_state,
                wng::GraphEditorCommandId::ClearSelection);
        assert(command.result == wng::Result::InvalidArgument);
        assert(!command.success());
        assert(!command.changed);
        assert(!command.before.clear_selection);
        assert(!command.after.clear_selection);

        command = wng::run_graph_editor_state_command(editor_state, invalid_command);
        assert(command.result == wng::Result::InvalidArgument);
        assert(!command.success());
        assert(!command.changed);
        assert(!command.before.any_available());
        assert(!command.after.any_available());

        const wng::NodeId node { 11U };
        const wng::PortId source_port { 21U };
        const wng::PortId target_port { 22U };

        assert(editor_state.select_node(node) == wng::Result::Ok);
        wng::GraphEditorCommandAvailability availability =
            wng::graph_editor_command_availability(editor_state);
        assert(wng::graph_editor_command_available(
            availability,
            wng::GraphEditorCommandId::ClearSelection));
        assert(wng::graph_editor_command_available(
            editor_state,
            wng::GraphEditorCommandId::ClearSelection));

        command = wng::run_graph_editor_state_command(
            editor_state,
            wng::GraphEditorCommandId::ClearSelection);
        assert(command.success());
        assert(command.changed);
        assert(command.before.clear_selection);
        assert(!command.after.clear_selection);
        assert(!wng::graph_editor_has_selection(editor_state));

        assert(editor_state.set_hovered_port(target_port) == wng::Result::Ok);
        assert(wng::graph_editor_command_available(
            editor_state,
            wng::GraphEditorCommandId::ClearHover));
        command = wng::run_graph_editor_state_command(
            editor_state,
            wng::GraphEditorCommandId::ClearHover);
        assert(command.success());
        assert(command.changed);
        assert(command.before.clear_hover);
        assert(!command.after.clear_hover);
        assert(!wng::graph_editor_has_hover(editor_state));

        assert(editor_state.begin_pending_link(source_port) == wng::Result::Ok);
        assert(editor_state.set_pending_link_target(target_port) == wng::Result::Ok);
        assert(wng::graph_editor_command_available(
            editor_state,
            wng::GraphEditorCommandId::CancelPendingLink));
        command = wng::run_graph_editor_state_command(
            editor_state,
            wng::GraphEditorCommandId::CancelPendingLink);
        assert(command.success());
        assert(command.changed);
        assert(command.before.cancel_pending_link);
        assert(command.before.complete_pending_link);
        assert(!command.after.cancel_pending_link);
        assert(!command.after.complete_pending_link);
        assert(!wng::graph_editor_has_active_pending_link(editor_state));
    }
}

int main()
{
    const std::vector<std::string> headers {
        "include/wng/dirty_propagation.hpp",
        "include/wng/execution_plan.hpp",
        "include/wng/ids.hpp",
        "include/wng/math.hpp",
        "include/wng/result.hpp",
        "include/wng/node.hpp",
        "include/wng/port.hpp",
        "include/wng/link.hpp",
        "include/wng/mutation_summary.hpp",
        "include/wng/serialization_dto.hpp",
        "include/wng/serialization.hpp",
        "include/wng/schema.hpp",
        "include/wng/schema_snapshot.hpp",
        "include/wng/schema_diff.hpp",
        "include/wng/schema_compatibility.hpp",
        "include/wng/schema_migration_plan.hpp",
        "include/wng/schema_migration_policy.hpp",
        "include/wng/schema_migration_apply_preview.hpp",
        "include/wng/schema_migration_command_preview.hpp",
        "include/wng/schema_migration_apply.hpp",
        "include/wng/schema_migration_apply_command.hpp",
        "include/wng/schema_migration_apply_command_history.hpp",
        "include/wng/schema_mutation.hpp",
        "include/wng/schema_validation.hpp",
        "include/wng/validation.hpp",
        "include/wng/graph.hpp",
        "include/wng/graph_command.hpp",
        "include/wng/graph_command_history.hpp",
        "include/wng/graph_history.hpp",
        "include/wng/graph_session.hpp",
        "include/wng/graph_editor_state.hpp",
        "include/wng/graph_editor_state_cleanup.hpp",
        "include/wng/graph_editor_selection_commands.hpp",
        "include/wng/graph_hit_testing.hpp",
        "include/wng/graph_editor_hit_testing.hpp",
        "include/wng/graph_editor_pending_link_state.hpp",
        "include/wng/graph_command_transaction.hpp",
        "include/wng/graph_diff.hpp",
        "include/wng/graph_mutation_preview.hpp",
        "include/wng/graph_restore.hpp",
        "include/wng/graph_snapshot.hpp",
        "include/wng/graph_redo.hpp",
        "include/wng/graph_undo.hpp",
        "include/wng/graph_validation.hpp",
        "include/wng/graph_traversal.hpp",
        "include/wng/wng.hpp"
    };

    for (const std::string& header : headers) {
        const std::string text = read_file(header);

        require_absent(text, "<wpl/");
        require_absent(text, "\"wpl/");
        require_absent(text, "<WPL/");
        require_absent(text, "\"WPL/");
        require_absent(text, "<X11/");
        require_absent(text, "<x11/");
        require_absent(text, "Xlib");
        require_absent(text, "<linux/");
        require_absent(text, "<sys/");
        require_absent(text, "<windows.h>");
        require_absent(text, "<Windows.h>");
        require_absent(text, "<functional>");
    }

    assert_editor_state_availability();
    assert_editor_state_set_commands();
    assert_editor_state_clear_commands();
    assert_editor_state_command_status();
    assert_editor_state_command_dispatch();

    return 0;
}
