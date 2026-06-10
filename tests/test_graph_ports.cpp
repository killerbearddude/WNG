#include <cassert>

#include <wng/wng.hpp>

int main()
{
    wng::Graph graph;
    wng::NodeId node;
    assert(graph.create_node(wng::NodeDesc{}, &node) == wng::Result::Ok);

    wng::PortDesc input_desc;
    input_desc.kind = wng::PortKind::Input;
    input_desc.name = "in";
    input_desc.type = "float";

    wng::PortId input;
    assert(graph.add_port(node, input_desc, nullptr) == wng::Result::InvalidArgument);
    assert(graph.add_port(wng::NodeId{}, input_desc, &input) == wng::Result::InvalidArgument);
    assert(graph.add_port(wng::NodeId{99}, input_desc, &input) == wng::Result::NotFound);

    wng::PortDesc bad_kind = input_desc;
    bad_kind.kind = static_cast<wng::PortKind>(99);
    wng::PortId unchanged{88};
    assert(graph.add_port(node, bad_kind, &unchanged) == wng::Result::InvalidArgument);
    assert(unchanged.value == 88U);

    assert(graph.add_port(node, input_desc, &input) == wng::Result::Ok);
    assert(input.value == 1U);
    assert(graph.find_port(input) != nullptr);
    assert(graph.find_node(node)->inputs.size() == 1U);
    assert(graph.find_node(node)->inputs[0] == input);

    wng::PortDesc output_desc;
    output_desc.kind = wng::PortKind::Output;
    wng::PortId output;
    assert(graph.add_port(node, output_desc, &output) == wng::Result::Ok);
    assert(output.value == 2U);
    assert(graph.find_node(node)->outputs.size() == 1U);
    assert(graph.find_node(node)->outputs[0] == output);

    wng::GraphMutationSummary summary;
    assert(graph.remove_port(wng::PortId{}, &summary) == wng::Result::InvalidArgument);
    assert(graph.remove_port(wng::PortId{42}, &summary) == wng::Result::NotFound);
    assert(graph.remove_port(input, &summary) == wng::Result::Ok);
    assert(summary.removed_nodes.empty());
    assert(summary.removed_ports.size() == 1U);
    assert(summary.removed_ports[0] == input);
    assert(summary.removed_links.empty());
    assert(graph.find_port(input) == nullptr);
    assert(graph.find_node(node)->inputs.empty());

    wng::PortId next;
    assert(graph.add_port(node, input_desc, &next) == wng::Result::Ok);
    assert(next.value == 3U);

    return 0;
}
