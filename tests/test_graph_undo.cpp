// Exercises inverse application for command records and batches.
// These tests protect the narrow undo primitive without introducing undo/redo
// stacks, command history ownership, redo, editor state, or WPL integration.

#include <cassert>
#include <vector>

#include <wng/graph_undo.hpp>
#include <wng/graph_validation.hpp>
#include <wng/schema.hpp>

namespace
{
    wng::NodeDesc make_node_desc(const char* title = "Node", const char* type = "schema.node")
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = title;
        desc.position = wng::Vec2 { 1.0f, 2.0f };
        desc.size = wng::Vec2 { 120.0f, 60.0f };
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

    bool node_has_input(const wng::Graph& graph, wng::NodeId node, wng::PortId port)
    {
        const wng::Node* graph_node = graph.find_node(node);
        assert(graph_node != nullptr);
        for (wng::PortId existing : graph_node->inputs) {
            if (existing == port) {
                return true;
            }
        }
        return false;
    }

    wng::PortDefinition input_definition(const char* name = "in")
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Input;
        definition.type = "number";
        definition.required = true;
        return definition;
    }

    wng::PortDefinition output_definition(const char* name = "out")
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Output;
        definition.type = "number";
        definition.required = true;
        return definition;
    }

    wng::GraphSchema make_schema()
    {
        wng::NodeDefinition definition;
        definition.type = "schema.node";
        definition.display_name = "Schema Node";
        definition.inputs.push_back(input_definition());
        definition.outputs.push_back(output_definition());

        wng::GraphSchema schema;
        assert(schema.add_node_definition(definition) == wng::Result::Ok);
        return schema;
    }

    struct Chain {
        wng::Graph graph;
        wng::NodeId a;
        wng::NodeId b;
        wng::NodeId c;
        wng::PortId a_output;
        wng::PortId b_input;
        wng::PortId b_output;
        wng::PortId c_input;
        wng::LinkId ab;
        wng::LinkId bc;
    };

    Chain make_chain()
    {
        Chain chain;
        chain.a = create_node(chain.graph, "A");
        chain.b = create_node(chain.graph, "B");
        chain.c = create_node(chain.graph, "C");
        chain.a_output = add_port(chain.graph, chain.a, wng::PortKind::Output, "a_out");
        chain.b_input = add_port(chain.graph, chain.b, wng::PortKind::Input, "b_in");
        chain.b_output = add_port(chain.graph, chain.b, wng::PortKind::Output, "b_out");
        chain.c_input = add_port(chain.graph, chain.c, wng::PortKind::Input, "c_in");
        chain.ab = create_link(chain.graph, chain.a_output, chain.b_input);
        chain.bc = create_link(chain.graph, chain.b_output, chain.c_input);
        return chain;
    }
}

int main()
{
    {
        // Failed command records are diagnostics, not safe undo units. Undo must
        // reject them and leave the graph unchanged.
        wng::Graph graph;
        const wng::GraphCommandResult failed =
            wng::command_add_port(graph, wng::NodeId { 999 }, make_port_desc(wng::PortKind::Input, "in"));

        const wng::GraphUndoResult undo = wng::undo_command(graph, failed.record);

        assert(undo.result == wng::Result::InvalidArgument);
        assert(!undo.success());
        assert(undo.applied_records.empty());
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
        assert(graph.links().empty());
    }

    {
        // Undoing a create-node command removes the created node through Graph
        // APIs rather than trying to replay construction in reverse.
        wng::Graph graph;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("Created"));
        assert(create.result == wng::Result::Ok);

        const wng::NodeId node = create.record.node;
        const wng::GraphUndoResult undo = wng::undo_command(graph, create.record);

        assert(undo.result == wng::Result::Ok);
        assert(undo.applied_records.size() == 1U);
        assert(graph.find_node(node) == nullptr);
        assert(wng::validate_graph(graph).valid());
    }

    {
        // Undoing an add-port command removes the created port and updates the
        // owning node's input/output vectors through Graph::remove_port.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph);
        const wng::GraphCommandResult add =
            wng::command_add_port(graph, node, make_port_desc(wng::PortKind::Input, "in"));
        assert(add.result == wng::Result::Ok);

        const wng::PortId port = add.record.port;
        assert(node_has_input(graph, node, port));

        const wng::GraphUndoResult undo = wng::undo_command(graph, add.record);

        assert(undo.result == wng::Result::Ok);
        assert(graph.find_port(port) == nullptr);
        assert(!node_has_input(graph, node, port));
    }

    {
        // Undoing a create-link command removes only the link; endpoint nodes and
        // ports remain available for future graph work.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "A");
        const wng::NodeId b = create_node(graph, "B");
        const wng::PortId output = add_port(graph, a, wng::PortKind::Output, "out");
        const wng::PortId input = add_port(graph, b, wng::PortKind::Input, "in");
        const wng::GraphCommandResult create = wng::command_create_link(graph, output, input);
        assert(create.result == wng::Result::Ok);

        const wng::LinkId link = create.record.link;
        const wng::GraphUndoResult undo = wng::undo_command(graph, create.record);

        assert(undo.result == wng::Result::Ok);
        assert(graph.find_link(link) == nullptr);
        assert(graph.find_port(output) != nullptr);
        assert(graph.find_port(input) != nullptr);
    }

    {
        // Undoing a destroy-link command restores the exact LinkId and endpoints
        // captured by the command record.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "A");
        const wng::NodeId b = create_node(graph, "B");
        const wng::PortId output = add_port(graph, a, wng::PortKind::Output, "out");
        const wng::PortId input = add_port(graph, b, wng::PortKind::Input, "in");
        const wng::LinkId link = create_link(graph, output, input);

        const wng::GraphCommandResult destroy = wng::command_destroy_link(graph, link);
        assert(destroy.result == wng::Result::Ok);
        assert(graph.find_link(link) == nullptr);

        const wng::GraphUndoResult undo = wng::undo_command(graph, destroy.record);

        assert(undo.result == wng::Result::Ok);
        const wng::Link* restored = graph.find_link(link);
        assert(restored != nullptr);
        assert(restored->from == output);
        assert(restored->to == input);
    }

    {
        // Undoing a remove-port command restores both the port and dependent link
        // snapshots with stable IDs.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "A");
        const wng::NodeId b = create_node(graph, "B");
        const wng::PortId output = add_port(graph, a, wng::PortKind::Output, "out");
        const wng::PortId input = add_port(graph, b, wng::PortKind::Input, "in");
        const wng::LinkId link = create_link(graph, output, input);

        const wng::GraphCommandResult remove = wng::command_remove_port(graph, input);
        assert(remove.result == wng::Result::Ok);
        assert(graph.find_port(input) == nullptr);
        assert(graph.find_link(link) == nullptr);

        const wng::GraphUndoResult undo = wng::undo_command(graph, remove.record);

        assert(undo.result == wng::Result::Ok);
        assert(graph.find_port(input) != nullptr);
        assert(graph.find_link(link) != nullptr);
        assert(node_has_input(graph, b, input));
    }

    {
        // Undoing a destroy-node command restores the removed node, owned ports,
        // and connected links with their original stable IDs.
        Chain chain = make_chain();
        const wng::GraphCommandResult destroy =
            wng::command_destroy_node(chain.graph, chain.b);
        assert(destroy.result == wng::Result::Ok);
        assert(chain.graph.find_node(chain.b) == nullptr);

        const wng::GraphUndoResult undo = wng::undo_command(chain.graph, destroy.record);

        assert(undo.result == wng::Result::Ok);
        assert(chain.graph.find_node(chain.b) != nullptr);
        assert(chain.graph.find_port(chain.b_input) != nullptr);
        assert(chain.graph.find_port(chain.b_output) != nullptr);
        assert(chain.graph.find_link(chain.ab) != nullptr);
        assert(chain.graph.find_link(chain.bc) != nullptr);
        assert(wng::validate_graph(chain.graph).valid());
    }

    {
        // Schema-aware create commands undo from recorded graph effects. No schema
        // object is needed when applying the inverse mutation.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, schema, make_node_desc("Schema Created"));
        assert(create.result == wng::Result::Ok);

        const wng::NodeId node = create.record.node;
        const wng::GraphUndoResult undo = wng::undo_command(graph, create.record);

        assert(undo.result == wng::Result::Ok);
        assert(graph.find_node(node) == nullptr);
    }

    {
        // Schema instantiation records created ports as graph effects, and undo
        // removes those ports before removing the created node.
        wng::Graph graph;
        const wng::GraphSchema schema = make_schema();
        const wng::GraphCommandResult instantiate =
            wng::command_instantiate_node(graph, schema, make_node_desc("Schema Instance"));
        assert(instantiate.result == wng::Result::Ok);

        const wng::NodeId node = instantiate.record.node;
        assert(!instantiate.record.created_ports.empty());

        const wng::GraphUndoResult undo = wng::undo_command(graph, instantiate.record);

        assert(undo.result == wng::Result::Ok);
        assert(graph.find_node(node) == nullptr);
        assert(graph.ports().empty());
    }

    {
        // Batch undo walks records in reverse order so links are removed before
        // their ports and nodes. This protects dependency-safe batch undo.
        wng::Graph graph;
        wng::GraphCommandBatch batch;

        const wng::GraphCommandResult create_a =
            wng::command_create_node(graph, make_node_desc("A"));
        wng::append_command_result(batch, create_a);
        const wng::GraphCommandResult create_b =
            wng::command_create_node(graph, make_node_desc("B"));
        wng::append_command_result(batch, create_b);
        const wng::GraphCommandResult add_output =
            wng::command_add_port(graph, create_a.record.node, make_port_desc(wng::PortKind::Output, "out"));
        wng::append_command_result(batch, add_output);
        const wng::GraphCommandResult add_input =
            wng::command_add_port(graph, create_b.record.node, make_port_desc(wng::PortKind::Input, "in"));
        wng::append_command_result(batch, add_input);
        const wng::GraphCommandResult create_link =
            wng::command_create_link(graph, add_output.record.port, add_input.record.port);
        wng::append_command_result(batch, create_link);

        assert(batch.result == wng::Result::Ok);
        assert(!graph.nodes().empty());
        assert(!graph.ports().empty());
        assert(!graph.links().empty());

        const wng::GraphUndoResult undo = wng::undo_command_batch(graph, batch);

        assert(undo.result == wng::Result::Ok);
        assert(undo.applied_records.size() == batch.records.size());
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
        assert(graph.links().empty());
    }

    {
        // Failed batches are diagnostic metadata, not safe undo units. The graph
        // must remain unchanged.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph);
        wng::GraphCommandBatch batch;
        batch.result = wng::Result::InvalidArgument;

        const wng::GraphUndoResult undo = wng::undo_command_batch(graph, batch);

        assert(undo.result == wng::Result::InvalidArgument);
        assert(undo.applied_records.empty());
        assert(graph.find_node(node) != nullptr);
        assert(graph.nodes().size() == 1U);
    }

    {
        // Batch undo can restore a destroyed subgraph because destructive command
        // records carry removed node, port, and link snapshots.
        Chain chain = make_chain();
        wng::GraphCommandBatch batch;
        const wng::GraphCommandResult destroy =
            wng::command_destroy_node(chain.graph, chain.b);
        wng::append_command_result(batch, destroy);
        assert(chain.graph.find_node(chain.b) == nullptr);

        const wng::GraphUndoResult undo = wng::undo_command_batch(chain.graph, batch);

        assert(undo.result == wng::Result::Ok);
        assert(chain.graph.find_node(chain.b) != nullptr);
        assert(chain.graph.find_port(chain.b_input) != nullptr);
        assert(chain.graph.find_port(chain.b_output) != nullptr);
        assert(chain.graph.find_link(chain.ab) != nullptr);
        assert(chain.graph.find_link(chain.bc) != nullptr);
    }

    {
        // Undo failure is atomic. If a created object is already missing, undo
        // returns NotFound and leaves the caller's graph exactly as it was.
        wng::Graph graph;
        const wng::GraphCommandResult create =
            wng::command_create_node(graph, make_node_desc("Created"));
        assert(create.result == wng::Result::Ok);

        wng::GraphMutationSummary summary;
        assert(graph.destroy_node(create.record.node, &summary) == wng::Result::Ok);
        assert(graph.nodes().empty());

        const wng::GraphUndoResult undo = wng::undo_command(graph, create.record);

        assert(undo.result == wng::Result::NotFound);
        assert(undo.applied_records.empty());
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
        assert(graph.links().empty());
    }

    {
        // Undo restores high-ID snapshots through graph_restore, and normal Graph
        // creation after undo must not collide with the restored stable ID.
        wng::Graph graph;
        wng::GraphCommandRecord record;
        record.result = wng::Result::Ok;
        record.removed_nodes.push_back(wng::Node {});
        record.removed_nodes[0].id = wng::NodeId { 100 };
        record.removed_nodes[0].title = "High Id";
        record.removed_nodes[0].size = wng::Vec2 { 10.0f, 10.0f };

        const wng::GraphUndoResult undo = wng::undo_command(graph, record);
        assert(undo.result == wng::Result::Ok);
        assert(graph.find_node(wng::NodeId { 100 }) != nullptr);

        wng::NodeId next;
        assert(graph.create_node(make_node_desc("Next"), &next) == wng::Result::Ok);
        assert(next.value > 100U);
    }

    return 0;
}
