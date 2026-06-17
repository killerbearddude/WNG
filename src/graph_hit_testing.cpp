// Implements WPL-free graph-space hit testing for graph objects.
// The implementation derives simple port anchors from existing node bounds and
// port ordering; it does not own rendering geometry, screen transforms, input
// routing, widgets, layout, or WPL integration.

#include <wng/graph_hit_testing.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

namespace
{
    float min_value(float lhs, float rhs)
    {
        return lhs < rhs ? lhs : rhs;
    }

    float max_value(float lhs, float rhs)
    {
        return lhs > rhs ? lhs : rhs;
    }

    float clamp_value(float value, float minimum, float maximum)
    {
        if (value < minimum) {
            return minimum;
        }
        if (value > maximum) {
            return maximum;
        }
        return value;
    }

    bool enabled_for_hit_test(const wng::Node& node, const wng::GraphHitTestOptions& options)
    {
        return options.include_disabled || node.enabled;
    }

    bool enabled_for_hit_test(const wng::Port& port, const wng::GraphHitTestOptions& options)
    {
        return options.include_disabled || port.enabled;
    }

    bool enabled_for_hit_test(const wng::Link& link, const wng::GraphHitTestOptions& options)
    {
        return options.include_disabled || link.enabled;
    }

    float squared_distance(wng::Vec2 lhs, wng::Vec2 rhs)
    {
        const float dx = lhs.x - rhs.x;
        const float dy = lhs.y - rhs.y;
        return dx * dx + dy * dy;
    }

    float squared_distance_to_segment(wng::Vec2 point, wng::Vec2 start, wng::Vec2 end)
    {
        const float vx = end.x - start.x;
        const float vy = end.y - start.y;
        const float wx = point.x - start.x;
        const float wy = point.y - start.y;
        const float length_squared = vx * vx + vy * vy;
        if (length_squared <= 0.0f) {
            return squared_distance(point, start);
        }

        const float t = clamp_value((wx * vx + wy * vy) / length_squared, 0.0f, 1.0f);
        const wng::Vec2 projection { start.x + t * vx, start.y + t * vy };
        return squared_distance(point, projection);
    }

    bool point_in_node(const wng::Node& node, wng::Vec2 point)
    {
        const float min_x = min_value(node.position.x, node.position.x + node.size.x);
        const float max_x = max_value(node.position.x, node.position.x + node.size.x);
        const float min_y = min_value(node.position.y, node.position.y + node.size.y);
        const float max_y = max_value(node.position.y, node.position.y + node.size.y);
        return point.x >= min_x && point.x <= max_x && point.y >= min_y && point.y <= max_y;
    }

    bool should_hit_node(const wng::Node& node, const wng::GraphHitTestOptions& options)
    {
        return node.visible && enabled_for_hit_test(node, options);
    }

    bool should_layout_port(const wng::Port& port, const wng::Node& node)
    {
        return port.visible && node.visible;
    }

    bool should_hit_port(
        const wng::Port& port,
        const wng::Node& node,
        const wng::GraphHitTestOptions& options)
    {
        return should_layout_port(port, node) &&
               enabled_for_hit_test(port, options) &&
               enabled_for_hit_test(node, options);
    }

    bool should_hit_link(
        const wng::Link& link,
        const wng::Port& from,
        const wng::Port& to,
        const wng::Node& from_node,
        const wng::Node& to_node,
        const wng::GraphHitTestOptions& options)
    {
        return link.visible && from.visible && to.visible && from_node.visible && to_node.visible &&
               enabled_for_hit_test(link, options) &&
               enabled_for_hit_test(from, options) &&
               enabled_for_hit_test(to, options) &&
               enabled_for_hit_test(from_node, options) &&
               enabled_for_hit_test(to_node, options);
    }

    const std::vector<wng::PortId>& node_ports_for_kind(const wng::Node& node, wng::PortKind kind)
    {
        return kind == wng::PortKind::Input ? node.inputs : node.outputs;
    }

    bool visible_port_slot(
        const wng::Graph& graph,
        const wng::Node& node,
        const wng::Port& port,
        std::size_t& out_index,
        std::size_t& out_count)
    {
        const std::vector<wng::PortId>& ids = node_ports_for_kind(node, port.kind);
        out_index = 0;
        out_count = 0;
        bool found = false;

        for (wng::PortId id : ids) {
            const wng::Port* candidate = graph.find_port(id);
            if (candidate == nullptr || candidate->kind != port.kind) {
                continue;
            }
            if (!should_layout_port(*candidate, node)) {
                continue;
            }

            if (candidate->id == port.id) {
                out_index = out_count;
                found = true;
            }
            ++out_count;
        }

        return found && out_count > 0;
    }

    bool port_anchor_position_impl(
        const wng::Graph& graph,
        const wng::Port& port,
        const wng::GraphHitTestOptions& options,
        wng::Vec2& out_position)
    {
        const wng::Node* node = graph.find_node(port.node);
        if (node == nullptr || !should_hit_port(port, *node, options)) {
            return false;
        }

        std::size_t index = 0;
        std::size_t count = 0;
        if (!visible_port_slot(graph, *node, port, index, count)) {
            return false;
        }

        const float min_x = min_value(node->position.x, node->position.x + node->size.x);
        const float max_x = max_value(node->position.x, node->position.x + node->size.x);
        const float min_y = min_value(node->position.y, node->position.y + node->size.y);
        const float max_y = max_value(node->position.y, node->position.y + node->size.y);
        const float height = max_y - min_y;
        const float slot = static_cast<float>(index + 1) / static_cast<float>(count + 1);

        out_position.x = port.kind == wng::PortKind::Input ? min_x : max_x;
        out_position.y = min_y + height * slot;
        return true;
    }
}

namespace wng
{
    bool GraphHitTestResult::hit() const
    {
        return kind != GraphHitTestKind::None;
    }

    bool graph_port_anchor_position(
        const Graph& graph,
        PortId port,
        const GraphHitTestOptions& options,
        Vec2* out_position)
    {
        if (out_position == nullptr) {
            return false;
        }

        const Port* found = graph.find_port(port);
        if (found == nullptr) {
            return false;
        }

        return port_anchor_position_impl(graph, *found, options, *out_position);
    }

    GraphHitTestResult hit_test_graph(
        const Graph& graph,
        Vec2 point,
        const GraphHitTestOptions& options)
    {
        GraphHitTestResult best_port;
        float best_port_distance = std::numeric_limits<float>::max();
        const float port_radius = options.port_radius < 0.0f ? 0.0f : options.port_radius;
        const float port_radius_squared = port_radius * port_radius;

        for (const Port& port : graph.ports()) {
            Vec2 anchor;
            if (!port_anchor_position_impl(graph, port, options, anchor)) {
                continue;
            }

            const float distance = squared_distance(point, anchor);
            if (distance <= port_radius_squared &&
                (best_port.kind == GraphHitTestKind::None ||
                 distance < best_port_distance ||
                 (distance == best_port_distance && port.id.value < best_port.port.value))) {
                best_port.kind = GraphHitTestKind::Port;
                best_port.port = port.id;
                best_port.node = port.node;
                best_port.link = LinkId {};
                best_port_distance = distance;
            }
        }

        if (best_port.kind != GraphHitTestKind::None) {
            return best_port;
        }

        GraphHitTestResult best_link;
        float best_link_distance = std::numeric_limits<float>::max();
        const float link_radius = options.link_radius < 0.0f ? 0.0f : options.link_radius;
        const float link_radius_squared = link_radius * link_radius;

        for (const Link& link : graph.links()) {
            const Port* from = graph.find_port(link.from);
            const Port* to = graph.find_port(link.to);
            if (from == nullptr || to == nullptr) {
                continue;
            }

            const Node* from_node = graph.find_node(from->node);
            const Node* to_node = graph.find_node(to->node);
            if (from_node == nullptr || to_node == nullptr ||
                !should_hit_link(link, *from, *to, *from_node, *to_node, options)) {
                continue;
            }

            Vec2 from_anchor;
            Vec2 to_anchor;
            if (!port_anchor_position_impl(graph, *from, options, from_anchor) ||
                !port_anchor_position_impl(graph, *to, options, to_anchor)) {
                continue;
            }

            const float distance = squared_distance_to_segment(point, from_anchor, to_anchor);
            if (distance <= link_radius_squared &&
                (best_link.kind == GraphHitTestKind::None ||
                 distance < best_link_distance ||
                 (distance == best_link_distance && link.id.value < best_link.link.value))) {
                best_link.kind = GraphHitTestKind::Link;
                best_link.link = link.id;
                best_link.node = NodeId {};
                best_link.port = PortId {};
                best_link_distance = distance;
            }
        }

        if (best_link.kind != GraphHitTestKind::None) {
            return best_link;
        }

        GraphHitTestResult best_node;
        for (const Node& node : graph.nodes()) {
            if (!should_hit_node(node, options) || !point_in_node(node, point)) {
                continue;
            }

            if (best_node.kind == GraphHitTestKind::None || node.id.value > best_node.node.value) {
                best_node.kind = GraphHitTestKind::Node;
                best_node.node = node.id;
                best_node.port = PortId {};
                best_node.link = LinkId {};
            }
        }

        return best_node;
    }
}
