// Exercises read-only editor action availability over GraphEditorState.
// This test avoids graph mutation, GraphSession, schema policy, UI, rendering,
// platform input, widgets, and WPL integration.

#include <cassert>

#include <wng/graph_editor_state.hpp>

namespace
{
    void assert_none_available(const wng::GraphEditorCommandAvailability& availability)
    {
        assert(!availability.clear_selection);
        assert(!availability.remove_selection);
        assert(!availability.clear_hover);
        assert(!availability.cancel_pending_link);
        assert(!availability.complete_pending_link);
        assert(!availability.any_available());
    }
}

int main()
{
    wng::GraphEditorState editor_state;
    wng::GraphEditorCommandAvailability availability =
        wng::graph_editor_command_availability(editor_state);
    assert_none_available(availability);

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
    assert_none_available(availability);

    return 0;
}
