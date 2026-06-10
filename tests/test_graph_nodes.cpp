#include <cassert>
#include <cmath>

#include <wng/wng.hpp>

int main()
{
    wng::Graph graph;
    wng::NodeDesc desc;
    desc.title = "Alpha";
    desc.position = {1.0f, 2.0f};
    desc.size = {100.0f, 50.0f};

    wng::NodeId node;
    assert(graph.create_node(desc, nullptr) == wng::Result::InvalidArgument);
    assert(graph.create_node(desc, &node) == wng::Result::Ok);
    assert(node.value == 1U);
    assert(graph.nodes().size() == 1U);
    assert(graph.find_node(node) != nullptr);
    assert(graph.find_node(node)->title == "Alpha");

    wng::NodeDesc invalid = desc;
    invalid.position.x = std::nanf("");
    wng::NodeId unchanged{99};
    assert(graph.create_node(invalid, &unchanged) == wng::Result::InvalidArgument);
    assert(unchanged.value == 99U);
    assert(graph.nodes().size() == 1U);

    invalid = desc;
    invalid.size.y = -1.0f;
    assert(graph.create_node(invalid, &unchanged) == wng::Result::InvalidArgument);
    assert(unchanged.value == 99U);

    wng::GraphMutationSummary summary;
    assert(graph.destroy_node(wng::NodeId{}, &summary) == wng::Result::InvalidArgument);
    assert(graph.destroy_node(wng::NodeId{42}, &summary) == wng::Result::NotFound);
    assert(graph.destroy_node(node, &summary) == wng::Result::Ok);
    assert(summary.removed_nodes.size() == 1U);
    assert(summary.removed_nodes[0] == node);
    assert(summary.removed_ports.empty());
    assert(summary.removed_links.empty());
    assert(graph.find_node(node) == nullptr);

    wng::NodeId next;
    assert(graph.create_node(desc, &next) == wng::Result::Ok);
    assert(next.value == 2U);

    return 0;
}
