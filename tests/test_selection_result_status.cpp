#include <cassert>

#include <wng/graph_editor_selection_commands.hpp>

namespace
{
    wng::NodeDesc make_node(const char* title)
    {
        wng::NodeDesc desc;
        desc.type = "test.node";
        desc.title = title;
        return desc;
    }

    wng::PortDesc make_port(wng::PortKind kind, const char* name)
    {
        wng::PortDesc desc;
        desc.kind = kind;
        desc.name = name;
        desc.type = "number";
        return desc;
    }

    struct SelectionContext {
        wng::GraphSession session;
        wng::NodeId source;
        wng::NodeId sink;
        wng::PortId source_out;
        wng::PortId sink_in;
        wng::LinkId link;
    };

    void build_context(SelectionContext& context)
    {
        assert(context.session.graph().create_node(make_node("source"), &context.source) ==
            wng::Result::Ok);
        assert(context.session.graph().create_node(make_node("sink"), &context.sink) ==
            wng::Result::Ok);
        assert(context.session.graph().add_port(
            context.source,
            make_port(wng::PortKind::Output, "out"),
            &context.source_out) == wng::Result::Ok);
        assert(context.session.graph().add_port(
            context.sink,
            make_port(wng::PortKind::Input, "in"),
            &context.sink_in) == wng::Result::Ok);
        assert(context.session.graph().create_link(
            context.source_out,
            context.sink_in,
            &context.link) == wng::Result::Ok);
    }

    void assert_delete_graph_objects_becomes_unavailable()
    {
        SelectionContext context;
        build_context(context);

        wng::GraphEditorState editor_state;
        assert(editor_state.select_link(context.link) == wng::Result::Ok);
        assert(editor_state.select_port(context.source_out) == wng::Result::Ok);
        assert(editor_state.select_node(context.source) == wng::Result::Ok);

        const wng::GraphEditorSelectionCommandResult result =
            wng::delete_selected_graph_objects(context.session, editor_state);
        assert(result.success());
        assert(result.deleted_count() == 3U);
        assert(result.before.live_count() == 3U);
        assert(result.after.live_count() == 0U);
        assert(result.after.selected_count() == 0U);

        const wng::GraphEditorSelectionCommandResultStatus status =
            wng::graph_editor_selection_command_result_status(
                result,
                wng::GraphEditorSelectionCommandId::DeleteSelectedGraphObjects);
        assert(status.availability_changed());
        assert(!status.became_available());
        assert(status.became_unavailable());
        assert(status.before.available);
        assert(!status.after.available);
        assert(status.after.unavailable_reason ==
            wng::GraphEditorSelectionCommandUnavailableReason::NoSelection);

        const wng::GraphEditorSelectionCommandStatus before_status =
            wng::graph_editor_selection_command_result_before_status(
                result,
                wng::GraphEditorSelectionCommandId::DeleteSelectedGraphObjects);
        const wng::GraphEditorSelectionCommandStatus after_status =
            wng::graph_editor_selection_command_result_after_status(
                result,
                wng::GraphEditorSelectionCommandId::DeleteSelectedGraphObjects);
        assert(before_status.available);
        assert(!after_status.available);
    }

    void assert_stale_selection_cleanup_becomes_no_selection()
    {
        SelectionContext context;
        build_context(context);

        wng::GraphEditorState editor_state;
        assert(editor_state.select_node(wng::NodeId { 9001U }) == wng::Result::Ok);

        const wng::GraphEditorSelectionCommandResult result =
            wng::delete_selected_nodes(context.session, editor_state);
        assert(result.success());
        assert(result.deleted_count() == 0U);
        assert(result.before.selected_count() == 1U);
        assert(result.before.only_stale_selection());
        assert(result.after.selected_count() == 0U);

        const wng::GraphEditorSelectionCommandResultStatus status =
            wng::graph_editor_selection_command_result_status(
                result,
                wng::GraphEditorSelectionCommandId::DeleteSelectedNodes);
        assert(!status.before.available);
        assert(!status.after.available);
        assert(!status.availability_changed());
        assert(status.before.unavailable_reason ==
            wng::GraphEditorSelectionCommandUnavailableReason::OnlyStaleSelection);
        assert(status.after.unavailable_reason ==
            wng::GraphEditorSelectionCommandUnavailableReason::NoSelection);
    }
}

int main()
{
    assert_delete_graph_objects_becomes_unavailable();
    assert_stale_selection_cleanup_becomes_no_selection();
    return 0;
}
