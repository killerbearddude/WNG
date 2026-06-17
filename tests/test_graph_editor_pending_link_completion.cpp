// Exercises pending-link completion through GraphSession without adding UI,
// rendering, WPL, pointer routing, widgets, or direct Graph mutation ownership.

#include <cassert>

#include <wng/graph_editor_pending_link_state.hpp>
#include <wng/graph_session.hpp>
#include <wng/schema.hpp>

namespace
{
    wng::PortDefinition input_definition(
        const char* name,
        const char* type = "number",
        bool enabled = true)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Input;
        definition.type = type;
        definition.required = true;
        definition.enabled = enabled;
        return definition;
    }

    wng::PortDefinition output_definition(
        const char* name,
        const char* type = "number",
        bool enabled = true)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Output;
        definition.type = type;
        definition.required = true;
        definition.enabled = enabled;
        return definition;
    }

    wng::NodeDefinition make_node_definition(bool input_enabled = true)
    {
        wng::NodeDefinition definition;
        definition.type = "math.add";
        definition.display_name = "Add";
        definition.inputs.push_back(input_definition("a", "number", input_enabled));
        definition.outputs.push_back(output_definition("result"));
        return definition;
    }

    wng::NodeDesc make_node_desc()
    {
        wng::NodeDesc desc;
        desc.type = "math.add";
        desc.title = "Add";
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

    void install_schema(wng::GraphSession& session, bool input_enabled = true)
    {
        assert(session.schema().add_node_definition(make_node_definition(input_enabled)) ==
               wng::Result::Ok);
    }

    wng::NodeId create_schema_node(wng::GraphSession& session)
    {
        const wng::GraphSessionCommandResult result = session.create_schema_node(make_node_desc());
        assert(result.success());
        assert(result.command.record.node != wng::NodeId {});
        return result.command.record.node;
    }

    wng::PortId add_schema_port(
        wng::GraphSession& session,
        wng::NodeId node,
        wng::PortKind kind,
        const char* name)
    {
        const wng::GraphSessionCommandResult result =
            session.add_schema_port(node, make_port_desc(kind, name));
        assert(result.success());
        assert(result.command.record.port != wng::PortId {});
        return result.command.record.port;
    }

    wng::PortId add_graph_port(
        wng::GraphSession& session,
        wng::NodeId node,
        wng::PortKind kind,
        const char* name)
    {
        const wng::GraphSessionCommandResult result =
            session.add_port(node, make_port_desc(kind, name));
        assert(result.success());
        assert(result.command.record.port != wng::PortId {});
        return result.command.record.port;
    }
}

int main()
{
    {
        // Inactive pending-link state is rejected without mutating the graph.
        wng::GraphSession session;
        install_schema(session);
        wng::GraphEditorState editor_state;

        const wng::GraphEditorPendingLinkCompleteResult result =
            wng::complete_graph_editor_pending_link(session, editor_state);

        assert(!result.success());
        assert(result.session.command.result == wng::Result::InvalidArgument);
        assert(result.session.command.record.kind == wng::GraphCommandKind::SchemaCreateLink);
        assert(session.graph().links().empty());
        assert(!editor_state.pending_link().active);
    }

    {
        // A source without a candidate target is rejected and preserves the active
        // pending-link state so the editor can keep the interaction alive.
        wng::GraphSession session;
        install_schema(session);
        const wng::NodeId source_node = create_schema_node(session);
        const wng::PortId output =
            add_schema_port(session, source_node, wng::PortKind::Output, "result");

        wng::GraphEditorState editor_state;
        assert(editor_state.begin_pending_link(output) == wng::Result::Ok);

        const wng::GraphEditorPendingLinkCompleteResult result =
            wng::complete_graph_editor_pending_link(session, editor_state);

        assert(!result.success());
        assert(result.session.command.result == wng::Result::InvalidArgument);
        assert(result.pending_link.active);
        assert(result.pending_link.from == output);
        assert(editor_state.pending_link().active);
        assert(editor_state.pending_link().from == output);
        assert(editor_state.pending_link().candidate_to == wng::PortId {});
        assert(session.graph().links().empty());
    }

    {
        // A valid source/candidate pair completes through the schema-aware session
        // command path, records a created link, and clears pending-link state.
        wng::GraphSession session;
        install_schema(session);
        const wng::NodeId source_node = create_schema_node(session);
        const wng::NodeId target_node = create_schema_node(session);
        const wng::PortId output =
            add_schema_port(session, source_node, wng::PortKind::Output, "result");
        const wng::PortId input =
            add_schema_port(session, target_node, wng::PortKind::Input, "a");

        wng::GraphEditorState editor_state;
        assert(editor_state.begin_pending_link(output) == wng::Result::Ok);
        assert(editor_state.set_pending_link_target(input) == wng::Result::Ok);

        const wng::GraphEditorPendingLinkCompleteResult result =
            wng::complete_graph_editor_pending_link(session, editor_state);

        assert(result.success());
        assert(result.session.command.result == wng::Result::Ok);
        assert(result.session.command.record.kind == wng::GraphCommandKind::SchemaCreateLink);
        assert(result.session.command.record.link != wng::LinkId {});
        assert(result.session.command.record.created_links.size() == 1U);
        assert(result.session.command.record.created_links[0].id ==
               result.session.command.record.link);
        assert(session.graph().find_link(result.session.command.record.link) != nullptr);
        assert(session.graph().links().size() == 1U);
        assert(result.pending_link.active);
        assert(result.pending_link.from == output);
        assert(result.pending_link.candidate_to == input);
        assert(!editor_state.pending_link().active);
        assert(editor_state.pending_link().from == wng::PortId {});
        assert(editor_state.pending_link().candidate_to == wng::PortId {});
    }

    {
        // Schema rejection leaves pending-link state intact and creates no link.
        // The ports exist in the graph, but the schema disables the target input.
        wng::GraphSession session;
        install_schema(session, false);
        const wng::NodeId source_node = create_schema_node(session);
        const wng::NodeId target_node = create_schema_node(session);
        const wng::PortId output =
            add_graph_port(session, source_node, wng::PortKind::Output, "result");
        const wng::PortId input =
            add_graph_port(session, target_node, wng::PortKind::Input, "a");

        wng::GraphEditorState editor_state;
        assert(editor_state.begin_pending_link(output) == wng::Result::Ok);
        assert(editor_state.set_pending_link_target(input) == wng::Result::Ok);

        const wng::GraphEditorPendingLinkCompleteResult result =
            wng::complete_graph_editor_pending_link(session, editor_state);

        assert(!result.success());
        assert(result.session.command.result == wng::Result::InvalidConnection);
        assert(result.session.command.record.kind == wng::GraphCommandKind::SchemaCreateLink);
        assert(result.session.command.record.link == wng::LinkId {});
        assert(result.session.command.record.created_links.empty());
        assert(session.graph().links().empty());
        assert(editor_state.pending_link().active);
        assert(editor_state.pending_link().from == output);
        assert(editor_state.pending_link().candidate_to == input);
    }

    return 0;
}
