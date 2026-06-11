// Exercises atomic graph object restoration from captured snapshots.
// These tests protect future undo/redo primitives without adding command replay,
// transaction rollback, schema-aware restore policy, or file I/O.

#include <cassert>
#include <limits>
#include <vector>

#include <wng/graph_restore.hpp>
#include <wng/graph_validation.hpp>

namespace
{
    wng::NodeDesc make_node_desc(const char* title = "Node")
    {
        wng::NodeDesc desc;
        desc.type = "restore.node";
        desc.title = title;
        desc.position = wng::Vec2 { 10.0f, 20.0f };
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
        const char* name,
        const char* type = "number")
    {
        wng::PortId port;
        assert(graph.add_port(node, make_port_desc(kind, name, type), &port) == wng::Result::Ok);
        return port;
    }

    wng::LinkId create_link(wng::Graph& graph, wng::PortId from, wng::PortId to)
    {
        wng::LinkId link;
        assert(graph.create_link(from, to, &link) == wng::Result::Ok);
        return link;
    }

    bool node_has_input(const wng::Node* node, wng::PortId port)
    {
        assert(node != nullptr);
        for (wng::PortId input : node->inputs) {
            if (input == port) {
                return true;
            }
        }

        return false;
    }

    struct GraphCounts {
        std::vector<wng::NodeId> nodes;
        std::vector<wng::PortId> ports;
        std::vector<wng::LinkId> links;
    };

    GraphCounts capture_counts(const wng::Graph& graph)
    {
        GraphCounts counts;
        for (const wng::Node& node : graph.nodes()) {
            counts.nodes.push_back(node.id);
        }
        for (const wng::Port& port : graph.ports()) {
            counts.ports.push_back(port.id);
        }
        for (const wng::Link& link : graph.links()) {
            counts.links.push_back(link.id);
        }
        return counts;
    }

    void assert_counts_equal(const wng::Graph& graph, const GraphCounts& counts)
    {
        assert(graph.nodes().size() == counts.nodes.size());
        assert(graph.ports().size() == counts.ports.size());
        assert(graph.links().size() == counts.links.size());
        for (std::vector<wng::NodeId>::size_type i = 0; i < counts.nodes.size(); ++i) {
            assert(graph.nodes()[i].id == counts.nodes[i]);
        }
        for (std::vector<wng::PortId>::size_type i = 0; i < counts.ports.size(); ++i) {
            assert(graph.ports()[i].id == counts.ports[i]);
        }
        for (std::vector<wng::LinkId>::size_type i = 0; i < counts.links.size(); ++i) {
            assert(graph.links()[i].id == counts.links[i]);
        }
    }
}

int main()
{
    {
        // Empty snapshots are valid no-op restores. Future undo code can safely
        // pass an empty captured set without changing graph state.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph);
        const GraphCounts before = capture_counts(graph);

        const wng::GraphRestoreResult result =
            wng::restore_graph_objects(graph, wng::GraphObjectSnapshot {});

        assert(result.result == wng::Result::Ok);
        assert(result.success());
        assert(result.restored_nodes.empty());
        assert(result.restored_ports.empty());
        assert(result.restored_links.empty());
        assert(graph.find_node(node) != nullptr);
        assert_counts_equal(graph, before);
    }

    {
        // Restoring a removed link preserves the original LinkId and endpoints
        // instead of allocating a replacement identity.
        wng::Graph graph;
        const wng::NodeId source = create_node(graph, "A");
        const wng::NodeId target = create_node(graph, "B");
        const wng::PortId output = add_port(graph, source, wng::PortKind::Output, "out");
        const wng::PortId input = add_port(graph, target, wng::PortKind::Input, "in");
        const wng::LinkId link = create_link(graph, output, input);
        const wng::Link snapshot_link = *graph.find_link(link);

        wng::GraphMutationSummary summary;
        assert(graph.destroy_link(link, &summary) == wng::Result::Ok);

        wng::GraphObjectSnapshot snapshot;
        snapshot.links.push_back(snapshot_link);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::Ok);
        assert(result.restored_links.size() == 1U);
        assert(result.restored_links[0] == link);
        assert(graph.links().size() == 1U);
        assert(graph.find_link(link)->from == output);
        assert(graph.find_link(link)->to == input);
    }

    {
        // Restoring a removed port reattaches it to its owning node's input list,
        // preserving the original PortId and port metadata.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph);
        const wng::PortId port = add_port(graph, node, wng::PortKind::Input, "in", "string");
        const wng::Port snapshot_port = *graph.find_port(port);

        wng::GraphMutationSummary summary;
        assert(graph.remove_port(port, &summary) == wng::Result::Ok);

        wng::GraphObjectSnapshot snapshot;
        snapshot.ports.push_back(snapshot_port);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::Ok);
        assert(result.restored_ports.size() == 1U);
        assert(result.restored_ports[0] == port);
        assert(graph.find_port(port)->name == "in");
        assert(graph.find_port(port)->type == "string");
        assert(node_has_input(graph.find_node(node), port));
    }

    {
        // Restoring a removed input port with its dependent link repairs both the
        // node port list and the link endpoint relationship atomically.
        wng::Graph graph;
        const wng::NodeId source = create_node(graph, "A");
        const wng::NodeId target = create_node(graph, "B");
        const wng::PortId output = add_port(graph, source, wng::PortKind::Output, "out");
        const wng::PortId input = add_port(graph, target, wng::PortKind::Input, "in");
        const wng::LinkId link = create_link(graph, output, input);
        const wng::Port snapshot_port = *graph.find_port(input);
        const wng::Link snapshot_link = *graph.find_link(link);

        wng::GraphMutationSummary summary;
        assert(graph.remove_port(input, &summary) == wng::Result::Ok);

        wng::GraphObjectSnapshot snapshot;
        snapshot.ports.push_back(snapshot_port);
        snapshot.links.push_back(snapshot_link);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::Ok);
        assert(graph.find_port(input) != nullptr);
        assert(graph.find_link(link) != nullptr);
        assert(node_has_input(graph.find_node(target), input));
        assert(graph.find_link(link)->from == output);
        assert(graph.find_link(link)->to == input);
    }

    {
        // Restoring a removed middle node with its ports and connected links
        // preserves all stable IDs needed by future undo/redo references.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "A");
        const wng::NodeId b = create_node(graph, "B");
        const wng::NodeId c = create_node(graph, "C");
        const wng::PortId a_out = add_port(graph, a, wng::PortKind::Output, "a_out");
        const wng::PortId b_in = add_port(graph, b, wng::PortKind::Input, "b_in");
        const wng::PortId b_out = add_port(graph, b, wng::PortKind::Output, "b_out");
        const wng::PortId c_in = add_port(graph, c, wng::PortKind::Input, "c_in");
        const wng::LinkId left = create_link(graph, a_out, b_in);
        const wng::LinkId right = create_link(graph, b_out, c_in);

        wng::GraphObjectSnapshot snapshot;
        snapshot.nodes.push_back(*graph.find_node(b));
        snapshot.ports.push_back(*graph.find_port(b_in));
        snapshot.ports.push_back(*graph.find_port(b_out));
        snapshot.links.push_back(*graph.find_link(left));
        snapshot.links.push_back(*graph.find_link(right));

        wng::GraphMutationSummary summary;
        assert(graph.destroy_node(b, &summary) == wng::Result::Ok);

        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::Ok);
        assert(graph.find_node(b) != nullptr);
        assert(graph.find_port(b_in) != nullptr);
        assert(graph.find_port(b_out) != nullptr);
        assert(graph.find_link(left) != nullptr);
        assert(graph.find_link(right) != nullptr);
        assert(graph.find_node(a) != nullptr);
        assert(graph.find_node(c) != nullptr);
        assert(wng::validate_graph(graph).valid());
    }

    {
        // Zero object IDs are invalid restoration input because they are not
        // persistent graph identities.
        wng::Graph graph;
        const GraphCounts before = capture_counts(graph);
        wng::Node node;
        node.id = wng::NodeId {};
        node.size = wng::Vec2 { 10.0f, 10.0f };

        wng::GraphObjectSnapshot snapshot;
        snapshot.nodes.push_back(node);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::InvalidArgument);
        assert_counts_equal(graph, before);
    }

    {
        // Snapshot IDs that collide with live graph IDs are rejected so restore
        // cannot overwrite existing objects.
        wng::Graph graph;
        const wng::NodeId live = create_node(graph);
        const GraphCounts before = capture_counts(graph);
        wng::Node node = *graph.find_node(live);

        wng::GraphObjectSnapshot snapshot;
        snapshot.nodes.push_back(node);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::AlreadyExists);
        assert_counts_equal(graph, before);
    }

    {
        // Duplicate IDs inside one snapshot are rejected before DTO replacement,
        // preventing ambiguous restore semantics.
        wng::Graph graph;
        const GraphCounts before = capture_counts(graph);
        wng::Node first;
        first.id = wng::NodeId { 10 };
        first.size = wng::Vec2 { 10.0f, 10.0f };
        wng::Node second = first;

        wng::GraphObjectSnapshot snapshot;
        snapshot.nodes.push_back(first);
        snapshot.nodes.push_back(second);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::AlreadyExists);
        assert_counts_equal(graph, before);
    }

    {
        // Ports cannot be restored without a live or restored parent node because
        // node ownership is part of the graph invariant.
        wng::Graph graph;
        const GraphCounts before = capture_counts(graph);
        wng::Port port;
        port.id = wng::PortId { 20 };
        port.node = wng::NodeId { 999 };
        port.kind = wng::PortKind::Input;

        wng::GraphObjectSnapshot snapshot;
        snapshot.ports.push_back(port);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::NotFound);
        assert_counts_equal(graph, before);
    }

    {
        // Links cannot be restored until both endpoint ports are live or restored.
        wng::Graph graph;
        const GraphCounts before = capture_counts(graph);
        wng::Link link;
        link.id = wng::LinkId { 30 };
        link.from = wng::PortId { 1 };
        link.to = wng::PortId { 2 };

        wng::GraphObjectSnapshot snapshot;
        snapshot.links.push_back(link);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::NotFound);
        assert_counts_equal(graph, before);
    }

    {
        // Invalid enum values are rejected explicitly instead of being routed into
        // node input/output vectors by accident.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph);
        const GraphCounts before = capture_counts(graph);
        wng::Port port;
        port.id = wng::PortId { 40 };
        port.node = node;
        port.kind = static_cast<wng::PortKind>(99);

        wng::GraphObjectSnapshot snapshot;
        snapshot.ports.push_back(port);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::InvalidArgument);
        assert_counts_equal(graph, before);
    }

    {
        // Invalid node geometry is rejected before restoration so DTO import never
        // receives a partially merged graph.
        wng::Graph graph;
        const GraphCounts before = capture_counts(graph);
        wng::Node node;
        node.id = wng::NodeId { 50 };
        node.size = wng::Vec2 { -1.0f, 10.0f };

        wng::GraphObjectSnapshot snapshot;
        snapshot.nodes.push_back(node);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::InvalidArgument);
        assert_counts_equal(graph, before);
    }

    {
        // Restoring a duplicate exact link is rejected with the same result class
        // used by DTO import and normal graph validation.
        wng::Graph graph;
        const wng::NodeId source = create_node(graph, "A");
        const wng::NodeId target = create_node(graph, "B");
        const wng::PortId output = add_port(graph, source, wng::PortKind::Output, "out");
        const wng::PortId input = add_port(graph, target, wng::PortKind::Input, "in");
        create_link(graph, output, input);
        const GraphCounts before = capture_counts(graph);
        wng::Link duplicate;
        duplicate.id = wng::LinkId { 99 };
        duplicate.from = output;
        duplicate.to = input;

        wng::GraphObjectSnapshot snapshot;
        snapshot.links.push_back(duplicate);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::AlreadyExists);
        assert_counts_equal(graph, before);
    }

    {
        // Multiple links into the same input remain invalid after restoration,
        // protecting dataflow single-input semantics.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "A");
        const wng::NodeId b = create_node(graph, "B");
        const wng::NodeId c = create_node(graph, "C");
        const wng::PortId a_out = add_port(graph, a, wng::PortKind::Output, "a_out");
        const wng::PortId b_out = add_port(graph, b, wng::PortKind::Output, "b_out");
        const wng::PortId c_in = add_port(graph, c, wng::PortKind::Input, "c_in");
        create_link(graph, a_out, c_in);
        const GraphCounts before = capture_counts(graph);
        wng::Link competing;
        competing.id = wng::LinkId { 100 };
        competing.from = b_out;
        competing.to = c_in;

        wng::GraphObjectSnapshot snapshot;
        snapshot.links.push_back(competing);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::InvalidConnection);
        assert_counts_equal(graph, before);
    }

    {
        // DTO-based restoration updates next-ID counters so later normal graph
        // creation never reuses restored high IDs.
        wng::Graph graph;
        wng::Node high;
        high.id = wng::NodeId { 100 };
        high.title = "High";
        high.size = wng::Vec2 { 10.0f, 10.0f };

        wng::GraphObjectSnapshot snapshot;
        snapshot.nodes.push_back(high);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);
        assert(result.result == wng::Result::Ok);

        wng::NodeId next;
        assert(graph.create_node(make_node_desc("Next"), &next) == wng::Result::Ok);
        assert(next.value > high.id.value);
    }

    {
        // Restore is atomic on failure: a valid snapshot node is not partially
        // published when another snapshot object is invalid.
        wng::Graph graph;
        const wng::NodeId live = create_node(graph, "Live");
        const GraphCounts before = capture_counts(graph);
        wng::Node valid;
        valid.id = wng::NodeId { 200 };
        valid.title = "Valid";
        valid.size = wng::Vec2 { 10.0f, 10.0f };
        wng::Port invalid;
        invalid.id = wng::PortId { 201 };
        invalid.node = wng::NodeId { 999 };
        invalid.kind = wng::PortKind::Input;

        wng::GraphObjectSnapshot snapshot;
        snapshot.nodes.push_back(valid);
        snapshot.ports.push_back(invalid);
        const wng::GraphRestoreResult result = wng::restore_graph_objects(graph, snapshot);

        assert(result.result == wng::Result::NotFound);
        assert(graph.find_node(valid.id) == nullptr);
        assert(graph.find_node(live) != nullptr);
        assert_counts_equal(graph, before);
    }

    return 0;
}
