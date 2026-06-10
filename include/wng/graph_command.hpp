// Provides command-style wrappers around core graph mutations.
// The command layer captures mutation results and inverse data needed by a
// future undo/redo stack, but it does not own command history.

#pragma once

#include <vector>

#include <wng/graph.hpp>
#include <wng/mutation_summary.hpp>
#include <wng/result.hpp>

namespace wng
{
    class GraphSchema;
}

namespace wng
{
    enum class GraphCommandKind {
        CreateNode,
        DestroyNode,
        AddPort,
        RemovePort,
        CreateLink,
        DestroyLink,
        SchemaCreateNode,
        SchemaInstantiateNode,
        SchemaAddPort,
        SchemaCreateLink
    };

    struct GraphCommandRecord {
        GraphCommandKind kind = GraphCommandKind::CreateNode;
        Result result = Result::Ok;

        NodeId node;
        PortId port;
        LinkId link;

        NodeDesc node_desc;
        PortDesc port_desc;

        std::vector<Node> created_nodes;
        std::vector<Port> created_ports;
        std::vector<Link> created_links;

        std::vector<Node> removed_nodes;
        std::vector<Port> removed_ports;
        std::vector<Link> removed_links;

        GraphMutationSummary summary;
    };

    struct GraphCommandResult {
        Result result = Result::Ok;
        GraphCommandRecord record;

        bool success() const;
    };

    // Executes Graph::create_node and records the created node id plus the
    // original descriptor needed to reason about future undo/redo behavior.
    GraphCommandResult command_create_node(
        Graph& graph,
        const NodeDesc& desc);

    // Executes Graph::destroy_node and records removed objects visible before
    // mutation. Future undo support can consume this value-oriented record.
    GraphCommandResult command_destroy_node(
        Graph& graph,
        NodeId node);

    // Executes Graph::add_port and records the created port id plus the original
    // descriptor. Port removal is intentionally a separate command.
    GraphCommandResult command_add_port(
        Graph& graph,
        NodeId node,
        const PortDesc& desc);

    // Executes Graph::remove_port and records the removed port plus dependent
    // links through GraphMutationSummary.
    GraphCommandResult command_remove_port(
        Graph& graph,
        PortId port);

    // Executes Graph::create_link and records the created link id.
    GraphCommandResult command_create_link(
        Graph& graph,
        PortId from,
        PortId to);

    // Executes Graph::destroy_link and records the removed link snapshot and
    // mutation summary.
    GraphCommandResult command_destroy_link(
        Graph& graph,
        LinkId link);

    // Executes schema-aware node creation and records the created node id plus the
    // descriptor. This helper preserves Graph's schema-free core by delegating to
    // the schema mutation layer.
    GraphCommandResult command_create_node(
        Graph& graph,
        const GraphSchema& schema,
        const NodeDesc& desc);

    // Executes schema-defined node instantiation and records created node and port
    // snapshots after success. Rollback summary is recorded on failure.
    GraphCommandResult command_instantiate_node(
        Graph& graph,
        const GraphSchema& schema,
        const NodeDesc& desc);

    // Executes schema-aware port creation and records the created port id plus the
    // descriptor. The schema layer decides whether the node type permits the port.
    GraphCommandResult command_add_port(
        Graph& graph,
        const GraphSchema& schema,
        NodeId node,
        const PortDesc& desc);

    // Executes schema-aware link creation and records the created link id.
    GraphCommandResult command_create_link(
        Graph& graph,
        const GraphSchema& schema,
        PortId from,
        PortId to);
}
