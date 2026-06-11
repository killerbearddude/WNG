// Exercises redo application from command records and command batches.
// These tests protect graph-effect replay without adding history ownership,
// redo stacks, editor state, schema policy, or WPL integration.

#include <cassert>
#include <vector>

#include <wng/graph_command.hpp>
#include <wng/graph_redo.hpp>
#include <wng/graph_undo.hpp>
#include <wng/graph_validation.hpp>
#include <wng/schema.hpp>

namespace
{
    wng::NodeDesc make_node_desc(const char* title = "Node")
    {
        wng::NodeDesc desc;
        desc.type = "math.node";
        desc.title = title;
        desc.size = wng::Vec2 { 100.0f, 40.0f };
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

    wng::NodeId create_node(wng::Graph& graph, const char* title = "Node")
    {
        wng::NodeId node;
        assert(graph.create_node(make_node_desc(title), &node) == wng::Result::Ok);
        return node;
    }

    wng::PortId add_port(
        wng::Graph& graph,
        wng::NodeId node,
        wng::PortKind kind,
        const char* name)
    {
        wng::PortId port;
        assert(graph.add_port(node, make_port_desc(kind, name), &port) == wng::Result::Ok);
        return port;
    }

    wng::LinkId create_link(wng::Graph& graph, wng::PortId from, wng::PortId to)
    {
        wng::LinkId link;
        assert(graph.create_link(from, to, &link) == wng::Result::Ok);
        return link;
    }

    bool node_exists(const wng::Graph& graph, wng::NodeId node)
    {
        return graph.find_node(node) != nullptr;
    }

    bool port_exists(const wng::Graph& graph, wng::PortId port)
    {
        return graph.find_port(port) != nullptr;
    }

    bool link_exists(const wng::Graph& graph, wng::LinkId link)
    {
        return graph.find_link(link) != nullptr;
    }

    bool node_has_input(const wng::Graph& graph, wng::NodeId node, wng::PortId port)
    {
        const wng::Node* graph_node = graph.find_node(node);
        assert(graph_node != nullptr);
        for (wng::PortId input : graph_node->inputs) {
            if (input == port) {
                return true;
            }
        }
        return false;
    }

    bool node_has_output(const wng::Graph& graph, wng::NodeId node, wng::PortId port)
    {
        const wng::Node* graph_node = graph.find_node(node);
        assert(graph_node != nullptr);
        for (wng::PortId output : graph_node->outputs) {
            if (output == port) {
                return true;
            }
        }
        return false;
    }

    wng::PortDefinition schema_input(const char* name)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Input;
        definition.type = "number";
        definition.required = true;
        return definition;
    }

    wng::PortDefinition schema_output(const char* name)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Output;
        definition.type = "number";
        definition.required = true;
        return definition;
    }

    wng::NodeDefinition schema_node_definition()
    {
        wng::NodeDefinition definition;
        definition.type = "math.node";
        definition.display_name = "Math Node";
        definition.inputs.push_back(schema_input("in"));
        definition.outputs.push_back(schema_output("out"));
        return definition;
    }

    wng::GraphSchema make_schema()
    {
        wng::GraphSchema schema;
        assert(schema.add_node_definition(schema_node_definition()) == wng::Result::Ok);
        return schema;
    }
}

int main()
{
    {
        // Failed command records are not safe redo units because they did not
        // produce a reliable graph effect to replay.
        wng::Graph graph;
        const wng::GraphCommandResult failed =
            wng::command_add_port(
                graph,
                wng::NodeId { 999 },
                make_port_desc(wng::PortKind::Input, "missing"));

        const wng::GraphRedoResult redo = wng::redo_command(graph, failed.record);

        assert(redo.result == wng::Result::InvalidArgument);
        assert(redo.applied_records.empty());
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
    }

    {
        // Redo restores created nodes with their original stable IDs instead of
        // replaying Graph::create_node, which would allocate fresh IDs.
        wng::Graph graph;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        const wng::NodeId node = create.record.node;

        assert(wng::undo_command(graph, create.record).success());
        assert(!node_exists(graph, node));

        const wng::GraphRedoResult redo = wng::redo_command(graph, create.record);

        assert(redo.result == wng::Result::Ok);
        assert(node_exists(graph, node));
        assert(redo.applied_records.size() == 1U);
        assert(wng::validate_graph(graph).valid());
    }

    {
        // Redo of add-port restores the port snapshot and reconnects it to the
        // parent node's input/output list.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "A");
        const wng::GraphCommandResult add =
            wng::command_add_port(graph, node, make_port_desc(wng::PortKind::Output, "out"));
        const wng::PortId port = add.record.port;

        assert(wng::undo_command(graph, add.record).success());
        assert(!port_exists(graph, port));

        const wng::GraphRedoResult redo = wng::redo_command(graph, add.record);

        assert(redo.result == wng::Result::Ok);
        assert(port_exists(graph, port));
        assert(node_has_output(graph, node, port));
    }

    {
        // Redo of create-link restores the link snapshot after undo while keeping
        // the original endpoint nodes and ports alive.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "A");
        const wng::NodeId b = create_node(graph, "B");
        const wng::PortId out = add_port(graph, a, wng::PortKind::Output, "out");
        const wng::PortId in = add_port(graph, b, wng::PortKind::Input, "in");
        const wng::GraphCommandResult create = wng::command_create_link(graph, out, in);
        const wng::LinkId link = create.record.link;

        assert(wng::undo_command(graph, create.record).success());
        assert(!link_exists(graph, link));

        const wng::GraphRedoResult redo = wng::redo_command(graph, create.record);

        assert(redo.result == wng::Result::Ok);
        assert(link_exists(graph, link));
        assert(graph.find_link(link)->from == out);
        assert(graph.find_link(link)->to == in);
        assert(node_exists(graph, a));
        assert(node_exists(graph, b));
    }

    {
        // Redo of destroy-link removes the restored link again but leaves the
        // endpoint nodes and ports intact.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "A");
        const wng::NodeId b = create_node(graph, "B");
        const wng::PortId out = add_port(graph, a, wng::PortKind::Output, "out");
        const wng::PortId in = add_port(graph, b, wng::PortKind::Input, "in");
        const wng::LinkId link = create_link(graph, out, in);
        const wng::GraphCommandResult destroy = wng::command_destroy_link(graph, link);

        assert(wng::undo_command(graph, destroy.record).success());
        assert(link_exists(graph, link));

        const wng::GraphRedoResult redo = wng::redo_command(graph, destroy.record);

        assert(redo.result == wng::Result::Ok);
        assert(!link_exists(graph, link));
        assert(port_exists(graph, out));
        assert(port_exists(graph, in));
    }

    {
        // Redo of remove-port removes both the restored port and its dependent
        // link, matching the original destructive graph effect.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "A");
        const wng::NodeId b = create_node(graph, "B");
        const wng::PortId out = add_port(graph, a, wng::PortKind::Output, "out");
        const wng::PortId in = add_port(graph, b, wng::PortKind::Input, "in");
        const wng::LinkId link = create_link(graph, out, in);
        const wng::GraphCommandResult remove = wng::command_remove_port(graph, in);

        assert(wng::undo_command(graph, remove.record).success());
        assert(port_exists(graph, in));
        assert(link_exists(graph, link));

        const wng::GraphRedoResult redo = wng::redo_command(graph, remove.record);

        assert(redo.result == wng::Result::Ok);
        assert(!port_exists(graph, in));
        assert(!link_exists(graph, link));
        assert(!node_has_input(graph, b, in));
    }

    {
        // Redo of destroy-node removes the restored node, its ports, and connected
        // links again while preserving unrelated neighbor nodes.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "A");
        const wng::NodeId b = create_node(graph, "B");
        const wng::NodeId c = create_node(graph, "C");
        const wng::PortId a_out = add_port(graph, a, wng::PortKind::Output, "a_out");
        const wng::PortId b_in = add_port(graph, b, wng::PortKind::Input, "b_in");
        const wng::PortId b_out = add_port(graph, b, wng::PortKind::Output, "b_out");
        const wng::PortId c_in = add_port(graph, c, wng::PortKind::Input, "c_in");
        const wng::LinkId ab = create_link(graph, a_out, b_in);
        const wng::LinkId bc = create_link(graph, b_out, c_in);
        const wng::GraphCommandResult destroy = wng::command_destroy_node(graph, b);

        assert(wng::undo_command(graph, destroy.record).success());
        assert(node_exists(graph, b));
        assert(link_exists(graph, ab));
        assert(link_exists(graph, bc));

        const wng::GraphRedoResult redo = wng::redo_command(graph, destroy.record);

        assert(redo.result == wng::Result::Ok);
        assert(!node_exists(graph, b));
        assert(!port_exists(graph, b_in));
        assert(!port_exists(graph, b_out));
        assert(!link_exists(graph, ab));
        assert(!link_exists(graph, bc));
        assert(node_exists(graph, a));
        assert(node_exists(graph, c));
        assert(wng::validate_graph(graph).valid());
    }

    {
        // Schema-created command records redo from captured effects only. Redo
        // does not require schema access and does not re-run schema policy.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, schema, make_node_desc("Schema"));
        const wng::NodeId node = create.record.node;

        assert(wng::undo_command(graph, create.record).success());
        assert(!node_exists(graph, node));

        const wng::GraphRedoResult redo = wng::redo_command(graph, create.record);

        assert(redo.result == wng::Result::Ok);
        assert(node_exists(graph, node));
        assert(graph.find_node(node)->type == "math.node");
    }

    {
        // Schema-instantiated records contain node and port snapshots, allowing
        // redo to restore the full instantiated object set without schema access.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();
        const wng::GraphCommandResult instantiate =
            wng::command_instantiate_node(graph, schema, make_node_desc("Instanced"));
        const wng::NodeId node = instantiate.record.node;
        assert(instantiate.record.created_ports.size() == 2U);
        const wng::PortId input = instantiate.record.created_ports[0].id;
        const wng::PortId output = instantiate.record.created_ports[1].id;

        assert(wng::undo_command(graph, instantiate.record).success());
        assert(!node_exists(graph, node));

        const wng::GraphRedoResult redo = wng::redo_command(graph, instantiate.record);

        assert(redo.result == wng::Result::Ok);
        assert(node_exists(graph, node));
        assert(port_exists(graph, input));
        assert(port_exists(graph, output));
    }

    {
        // Batch redo uses original command order so restored links can depend on
        // nodes and ports restored by earlier records.
        wng::Graph graph;
        wng::GraphCommandBatch batch;

        const wng::GraphCommandResult create_a = wng::command_create_node(graph, make_node_desc("A"));
        wng::append_command_result(batch, create_a);
        const wng::GraphCommandResult create_b = wng::command_create_node(graph, make_node_desc("B"));
        wng::append_command_result(batch, create_b);
        const wng::GraphCommandResult add_out =
            wng::command_add_port(graph, create_a.record.node, make_port_desc(wng::PortKind::Output, "out"));
        wng::append_command_result(batch, add_out);
        const wng::GraphCommandResult add_in =
            wng::command_add_port(graph, create_b.record.node, make_port_desc(wng::PortKind::Input, "in"));
        wng::append_command_result(batch, add_in);
        const wng::GraphCommandResult create_link =
            wng::command_create_link(graph, add_out.record.port, add_in.record.port);
        wng::append_command_result(batch, create_link);

        assert(wng::undo_command_batch(graph, batch).success());
        assert(graph.nodes().empty());

        const wng::GraphRedoResult redo = wng::redo_command_batch(graph, batch);

        assert(redo.result == wng::Result::Ok);
        assert(redo.applied_records.size() == 5U);
        assert(node_exists(graph, create_a.record.node));
        assert(node_exists(graph, create_b.record.node));
        assert(port_exists(graph, add_out.record.port));
        assert(port_exists(graph, add_in.record.port));
        assert(link_exists(graph, create_link.record.link));
        assert(wng::validate_graph(graph).valid());
    }

    {
        // Failed batches are rejected because partial-failure batch metadata is
        // not a safe redo unit.
        wng::Graph graph;
        const wng::NodeId original = create_node(graph, "Original");
        wng::GraphCommandBatch batch;
        batch.result = wng::Result::InvalidConnection;

        const wng::GraphRedoResult redo = wng::redo_command_batch(graph, batch);

        assert(redo.result == wng::Result::InvalidArgument);
        assert(redo.applied_records.empty());
        assert(node_exists(graph, original));
        assert(graph.nodes().size() == 1U);
    }

    {
        // Destructive batch redo re-applies a recorded subgraph deletion after
        // undo has restored the deleted objects.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "A");
        const wng::NodeId b = create_node(graph, "B");
        const wng::PortId out = add_port(graph, a, wng::PortKind::Output, "out");
        const wng::PortId in = add_port(graph, b, wng::PortKind::Input, "in");
        const wng::LinkId link = create_link(graph, out, in);
        wng::GraphCommandBatch batch;
        wng::append_command_result(batch, wng::command_destroy_node(graph, b));

        assert(wng::undo_command_batch(graph, batch).success());
        assert(node_exists(graph, b));
        assert(link_exists(graph, link));

        const wng::GraphRedoResult redo = wng::redo_command_batch(graph, batch);

        assert(redo.result == wng::Result::Ok);
        assert(node_exists(graph, a));
        assert(!node_exists(graph, b));
        assert(!link_exists(graph, link));
    }

    {
        // Redo is atomic on restore collision. If a created snapshot already
        // exists, redo fails and leaves the caller's graph unchanged.
        wng::Graph graph;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("A"));
        assert(wng::undo_command(graph, create.record).success());
        assert(wng::redo_command(graph, create.record).success());

        const std::vector<wng::Node> nodes_before = graph.nodes();
        const wng::GraphRedoResult redo_again = wng::redo_command(graph, create.record);

        assert(redo_again.result == wng::Result::AlreadyExists);
        assert(redo_again.applied_records.empty());
        assert(graph.nodes().size() == nodes_before.size());
        assert(graph.nodes()[0].id == nodes_before[0].id);
    }

    {
        // Redo of a high-ID created snapshot must preserve next-ID safety so a
        // later normal Graph creation cannot collide with restored IDs.
        wng::Graph graph;
        wng::GraphCommandRecord record;
        record.result = wng::Result::Ok;
        wng::Node node;
        node.id = wng::NodeId { 100 };
        node.type = "manual.high";
        node.title = "High";
        node.size = wng::Vec2 { 64.0f, 32.0f };
        record.created_nodes.push_back(node);

        const wng::GraphRedoResult redo = wng::redo_command(graph, record);
        assert(redo.result == wng::Result::Ok);
        assert(node_exists(graph, node.id));

        wng::NodeId next;
        assert(graph.create_node(make_node_desc("Next"), &next) == wng::Result::Ok);
        assert(next.value > node.id.value);
    }

    return 0;
}
