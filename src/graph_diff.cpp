// Implements deterministic, non-mutating graph diffing for WNG.
// Diffing compares stable graph object identities after validating both inputs;
// it does not repair graphs, apply patches, or interpret schema semantics.

#include <new>
#include <vector>

#include <wng/graph_diff.hpp>

#include <wng/graph_validation.hpp>

namespace
{
    wng::Result first_error_result(const wng::ValidationReport& report)
    {
        for (const wng::ValidationIssue& issue : report.issues) {
            if (issue.severity == wng::ValidationSeverity::Error) {
                return issue.result;
            }
        }

        return wng::Result::Ok;
    }

    const wng::Node* find_node_by_id(
        const std::vector<wng::Node>& nodes,
        wng::NodeId id)
    {
        for (const wng::Node& node : nodes) {
            if (node.id == id) {
                return &node;
            }
        }

        return nullptr;
    }

    const wng::Port* find_port_by_id(
        const std::vector<wng::Port>& ports,
        wng::PortId id)
    {
        for (const wng::Port& port : ports) {
            if (port.id == id) {
                return &port;
            }
        }

        return nullptr;
    }

    const wng::Link* find_link_by_id(
        const std::vector<wng::Link>& links,
        wng::LinkId id)
    {
        for (const wng::Link& link : links) {
            if (link.id == id) {
                return &link;
            }
        }

        return nullptr;
    }

    bool vec2_equal(wng::Vec2 a, wng::Vec2 b)
    {
        return a.x == b.x && a.y == b.y;
    }

    bool node_equal(const wng::Node& a, const wng::Node& b)
    {
        return a.id == b.id &&
            a.type == b.type &&
            a.title == b.title &&
            vec2_equal(a.position, b.position) &&
            vec2_equal(a.size, b.size) &&
            a.inputs == b.inputs &&
            a.outputs == b.outputs &&
            a.visible == b.visible &&
            a.enabled == b.enabled;
    }

    bool port_equal(const wng::Port& a, const wng::Port& b)
    {
        return a.id == b.id &&
            a.node == b.node &&
            a.kind == b.kind &&
            a.name == b.name &&
            a.type == b.type &&
            a.visible == b.visible &&
            a.enabled == b.enabled;
    }

    bool link_equal(const wng::Link& a, const wng::Link& b)
    {
        return a.id == b.id &&
            a.from == b.from &&
            a.to == b.to &&
            a.visible == b.visible &&
            a.enabled == b.enabled;
    }

    void append_node_diffs(
        const wng::Graph& before,
        const wng::Graph& after,
        wng::GraphDiff& diff)
    {
        const std::vector<wng::Node>& before_nodes = before.nodes();
        const std::vector<wng::Node>& after_nodes = after.nodes();

        // Ordering is deliberately phase-based and storage-order based so diffs
        // are reproducible: removed-before order, added-after order, then
        // modified-before order.
        for (const wng::Node& before_node : before_nodes) {
            if (find_node_by_id(after_nodes, before_node.id) == nullptr) {
                wng::NodeDiff entry;
                entry.change = wng::GraphDiffChange::Removed;
                entry.id = before_node.id;
                entry.before = before_node;
                diff.nodes.push_back(entry);
            }
        }

        for (const wng::Node& after_node : after_nodes) {
            if (find_node_by_id(before_nodes, after_node.id) == nullptr) {
                wng::NodeDiff entry;
                entry.change = wng::GraphDiffChange::Added;
                entry.id = after_node.id;
                entry.after = after_node;
                diff.nodes.push_back(entry);
            }
        }

        for (const wng::Node& before_node : before_nodes) {
            const wng::Node* after_node = find_node_by_id(after_nodes, before_node.id);
            if (after_node != nullptr && !node_equal(before_node, *after_node)) {
                wng::NodeDiff entry;
                entry.change = wng::GraphDiffChange::Modified;
                entry.id = before_node.id;
                entry.before = before_node;
                entry.after = *after_node;
                diff.nodes.push_back(entry);
            }
        }
    }

    void append_port_diffs(
        const wng::Graph& before,
        const wng::Graph& after,
        wng::GraphDiff& diff)
    {
        const std::vector<wng::Port>& before_ports = before.ports();
        const std::vector<wng::Port>& after_ports = after.ports();

        for (const wng::Port& before_port : before_ports) {
            if (find_port_by_id(after_ports, before_port.id) == nullptr) {
                wng::PortDiff entry;
                entry.change = wng::GraphDiffChange::Removed;
                entry.id = before_port.id;
                entry.before = before_port;
                diff.ports.push_back(entry);
            }
        }

        for (const wng::Port& after_port : after_ports) {
            if (find_port_by_id(before_ports, after_port.id) == nullptr) {
                wng::PortDiff entry;
                entry.change = wng::GraphDiffChange::Added;
                entry.id = after_port.id;
                entry.after = after_port;
                diff.ports.push_back(entry);
            }
        }

        for (const wng::Port& before_port : before_ports) {
            const wng::Port* after_port = find_port_by_id(after_ports, before_port.id);
            if (after_port != nullptr && !port_equal(before_port, *after_port)) {
                wng::PortDiff entry;
                entry.change = wng::GraphDiffChange::Modified;
                entry.id = before_port.id;
                entry.before = before_port;
                entry.after = *after_port;
                diff.ports.push_back(entry);
            }
        }
    }

    void append_link_diffs(
        const wng::Graph& before,
        const wng::Graph& after,
        wng::GraphDiff& diff)
    {
        const std::vector<wng::Link>& before_links = before.links();
        const std::vector<wng::Link>& after_links = after.links();

        for (const wng::Link& before_link : before_links) {
            if (find_link_by_id(after_links, before_link.id) == nullptr) {
                wng::LinkDiff entry;
                entry.change = wng::GraphDiffChange::Removed;
                entry.id = before_link.id;
                entry.before = before_link;
                diff.links.push_back(entry);
            }
        }

        for (const wng::Link& after_link : after_links) {
            if (find_link_by_id(before_links, after_link.id) == nullptr) {
                wng::LinkDiff entry;
                entry.change = wng::GraphDiffChange::Added;
                entry.id = after_link.id;
                entry.after = after_link;
                diff.links.push_back(entry);
            }
        }

        for (const wng::Link& before_link : before_links) {
            const wng::Link* after_link = find_link_by_id(after_links, before_link.id);
            if (after_link != nullptr && !link_equal(before_link, *after_link)) {
                wng::LinkDiff entry;
                entry.change = wng::GraphDiffChange::Modified;
                entry.id = before_link.id;
                entry.before = before_link;
                entry.after = *after_link;
                diff.links.push_back(entry);
            }
        }
    }
}

namespace wng
{
    bool GraphDiff::empty() const
    {
        return nodes.empty() && ports.empty() && links.empty();
    }

    bool GraphDiff::changed() const
    {
        return !empty();
    }

    bool GraphDiff::success() const
    {
        return result == Result::Ok;
    }

    GraphDiff diff_graphs(
        const Graph& before,
        const Graph& after)
    {
        GraphDiff diff;

        try {
            // Validation happens before comparison so the diff reports graph-core
            // model changes only for structurally valid graph snapshots.
            const ValidationReport before_report = validate_graph(before);
            if (!before_report.valid()) {
                const Result result = first_error_result(before_report);
                diff.result = result == Result::Ok ? Result::InvalidArgument : result;
                return diff;
            }

            const ValidationReport after_report = validate_graph(after);
            if (!after_report.valid()) {
                const Result result = first_error_result(after_report);
                diff.result = result == Result::Ok ? Result::InvalidArgument : result;
                return diff;
            }

            // Stable ID matching is the only identity rule. Titles, names,
            // endpoint structure, and storage index changes are treated as data,
            // not identity.
            append_node_diffs(before, after, diff);
            append_port_diffs(before, after, diff);
            append_link_diffs(before, after, diff);

            diff.result = Result::Ok;
            return diff;
        } catch (const std::bad_alloc&) {
            GraphDiff failure;
            failure.result = Result::AllocationFailure;
            return failure;
        }
    }
}
