// Provides WPL-free graph-space hit testing over WNG graph objects.
// This module returns stable IDs only and intentionally owns no screen/canvas
// transform, rendering, layout engine, widget logic, platform input, or WPL type.

#pragma once

#include <wng/graph.hpp>
#include <wng/ids.hpp>
#include <wng/math.hpp>

namespace wng
{
    // Identifies the category of graph object selected by a hit test. Priority is
    // controlled by hit_test_graph(): ports first, then links, then nodes.
    enum class GraphHitTestKind {
        None,
        Node,
        Port,
        Link
    };

    // Tunable graph-space hit-test distances. Values are expressed in the same
    // graph coordinate system as Node::position and Node::size.
    struct GraphHitTestOptions {
        float port_radius = 8.0f;
        float link_radius = 6.0f;
        bool include_disabled = true;
    };

    // Stable-ID result for one hit test. Only the ID matching kind is meaningful.
    struct GraphHitTestResult {
        GraphHitTestKind kind = GraphHitTestKind::None;
        NodeId node {};
        PortId port {};
        LinkId link {};

        bool hit() const;
    };

    // Hit tests visible ports, links, and nodes at a graph-space point. The
    // deterministic priority order is ports > links > nodes. Ports use a simple
    // editor-geometry convention: input anchors are on the left node edge, output
    // anchors are on the right node edge, and anchors are evenly spaced by visible
    // port order within each side.
    GraphHitTestResult hit_test_graph(
        const Graph& graph,
        Vec2 point,
        const GraphHitTestOptions& options = GraphHitTestOptions {});

    // Returns the deterministic graph-space anchor used by port and link hit
    // testing. The function returns false if the port or owning node is missing,
    // invisible, filtered by options, or cannot be assigned an anchor.
    bool graph_port_anchor_position(
        const Graph& graph,
        PortId port,
        const GraphHitTestOptions& options,
        Vec2* out_position);
}
