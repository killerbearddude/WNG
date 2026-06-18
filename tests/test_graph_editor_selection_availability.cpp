#include <cassert>

#include <wng/graph_editor_selection_commands.hpp>

namespace
{
    wng::NodeDesc node_desc(const char* title)
    {
        wng::NodeDesc desc;
        desc.type = "test.node";
        desc.title = title;
        return desc;
    }

    wng::PortDesc input_port(const char* name)
    {
        wng::PortDesc desc;
        desc.kind = wng::PortKind::Input;
        desc.name = name;
        desc.type = "number";
        return desc;
    }

    wng::PortDesc output_port(const char* name)
    {
        wng::PortDesc desc;
        desc.kind = wng::PortKind::Output;
        desc.name = name;
        desc.type = "number";
        return desc;
    }

    struct Fixture {
        wng::Graph graph;
        wng::NodeId source;
        wng::NodeId sink;
        wng::PortId source_out;
        wng::PortId sink_in;
        wng::LinkId link;
    };

    Fixture make_fixture()
    {
        Fixture fixture;
        assert(fixture.graph.create_node(node_desc("source"), &fixture.source) ==
            wng::Result::Ok);
        assert(fixture.graph.create_node(node_desc("sink"), &fixture.sink) ==
            wng::Result::Ok);
        assert(fixture.graph.add_port(fixture.source, output_port("out"), &fixture.source_out) ==
            wng::Result::Ok);
        assert(fixture.graph.add_port(fixture.sink, input_port("in"), &fixture.sink_in) ==
            wng::Result::Ok);
        assert(fixture.graph.create_link(fixture.source_out, fixture.sink_in, &fixture.link) ==
            wng::Result::Ok);
        return fixture;
    }

    void assert_empty_selection(const Fixture& fixture)
    {
        wng::GraphEditorState editor_state;
        const wng::GraphEditorSelectionCommandAvailability availability =
            wng::graph_editor_selection_command_availability(
                fixture.graph,
                editor_state);

        assert(availability.selected_count() == 0U);
        assert(availability.live_count() == 0U);
        assert(availability.stale_count() == 0U);
        assert(!availability.has_selection());
        assert(!availability.has_live_selection());
        assert(!availability.has_stale_selection());
        assert(!availability.only_stale_selection());
        assert(!availability.delete_selected_nodes());
        assert(!availability.delete_selected_ports());
        assert(!availability.delete_selected_links());
        assert(!availability.delete_selected_graph_objects());
        assert(!availability.any_delete_available());
    }

    void assert_stale_selection(const Fixture& fixture)
    {
        wng::GraphEditorState editor_state;
        assert(editor_state.select_node(wng::NodeId { 9001U }) == wng::Result::Ok);
        assert(editor_state.select_port(wng::PortId { 9002U }) == wng::Result::Ok);
        assert(editor_state.select_link(wng::LinkId { 9003U }) == wng::Result::Ok);

        const wng::GraphEditorSelectionCommandAvailability availability =
            wng::graph_editor_selection_command_availability(
                fixture.graph,
                editor_state);

        assert(availability.selected_nodes == 1U);
        assert(availability.selected_ports == 1U);
        assert(availability.selected_links == 1U);
        assert(availability.selected_count() == 3U);
        assert(availability.live_count() == 0U);
        assert(availability.stale_nodes == 1U);
        assert(availability.stale_ports == 1U);
        assert(availability.stale_links == 1U);
        assert(availability.stale_count() == 3U);
        assert(availability.has_selection());
        assert(!availability.has_live_selection());
        assert(availability.has_stale_selection());
        assert(availability.only_stale_selection());
        assert(!availability.any_delete_available());
    }

    void assert_mixed_selection(const Fixture& fixture)
    {
        wng::GraphEditorState editor_state;
        assert(editor_state.select_node(wng::NodeId { 9001U }) == wng::Result::Ok);
        assert(editor_state.select_port(wng::PortId { 9002U }) == wng::Result::Ok);
        assert(editor_state.select_link(wng::LinkId { 9003U }) == wng::Result::Ok);
        assert(editor_state.select_node(fixture.source) == wng::Result::Ok);
        assert(editor_state.select_port(fixture.source_out) == wng::Result::Ok);
        assert(editor_state.select_link(fixture.link) == wng::Result::Ok);

        const wng::GraphEditorSelectionCommandAvailability availability =
            wng::graph_editor_selection_command_availability(
                fixture.graph,
                editor_state);

        assert(availability.selected_count() == 6U);
        assert(availability.live_nodes == 1U);
        assert(availability.live_ports == 1U);
        assert(availability.live_links == 1U);
        assert(availability.live_count() == 3U);
        assert(availability.stale_count() == 3U);
        assert(availability.has_live_selection());
        assert(availability.has_stale_selection());
        assert(!availability.only_stale_selection());
        assert(availability.delete_selected_nodes());
        assert(availability.delete_selected_ports());
        assert(availability.delete_selected_links());
        assert(availability.delete_selected_graph_objects());
        assert(availability.any_delete_available());
    }
}

int main()
{
    const Fixture fixture = make_fixture();
    assert_empty_selection(fixture);
    assert_stale_selection(fixture);
    assert_mixed_selection(fixture);
    return 0;
}
