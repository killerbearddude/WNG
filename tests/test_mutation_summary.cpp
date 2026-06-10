#include <cassert>

#include <wng/wng.hpp>

int main()
{
    wng::Graph graph;
    wng::NodeId a;
    wng::NodeId b;
    assert(graph.create_node(wng::NodeDesc{}, &a) == wng::Result::Ok);
    assert(graph.create_node(wng::NodeDesc{}, &b) == wng::Result::Ok);

    wng::PortDesc input;
    input.kind = wng::PortKind::Input;
    wng::PortDesc output;
    output.kind = wng::PortKind::Output;

    wng::PortId a_in1;
    wng::PortId a_in2;
    wng::PortId a_out1;
    wng::PortId a_out2;
    wng::PortId b_in;
    wng::PortId b_out;
    assert(graph.add_port(a, input, &a_in1) == wng::Result::Ok);
    assert(graph.add_port(a, input, &a_in2) == wng::Result::Ok);
    assert(graph.add_port(a, output, &a_out1) == wng::Result::Ok);
    assert(graph.add_port(a, output, &a_out2) == wng::Result::Ok);
    assert(graph.add_port(b, input, &b_in) == wng::Result::Ok);
    assert(graph.add_port(b, output, &b_out) == wng::Result::Ok);

    wng::LinkId link1;
    wng::LinkId link2;
    assert(graph.create_link(a_out1, b_in, &link1) == wng::Result::Ok);
    assert(graph.create_link(b_out, a_in1, &link2) == wng::Result::Ok);

    wng::GraphMutationSummary failed;
    failed.removed_nodes.push_back(wng::NodeId{999});
    assert(graph.destroy_node(wng::NodeId{777}, &failed) == wng::Result::NotFound);
    assert(failed.removed_nodes.size() == 1U);
    assert(failed.removed_nodes[0].value == 999U);
    assert(graph.nodes().size() == 2U);
    assert(graph.ports().size() == 6U);
    assert(graph.links().size() == 2U);

    wng::GraphMutationSummary summary;
    assert(graph.destroy_node(a, &summary) == wng::Result::Ok);
    assert(summary.removed_nodes.size() == 1U);
    assert(summary.removed_nodes[0] == a);

    assert(summary.removed_ports.size() == 4U);
    assert(summary.removed_ports[0] == a_in1);
    assert(summary.removed_ports[1] == a_in2);
    assert(summary.removed_ports[2] == a_out1);
    assert(summary.removed_ports[3] == a_out2);

    assert(summary.removed_links.size() == 2U);
    assert(summary.removed_links[0] == link1);
    assert(summary.removed_links[1] == link2);

    assert(graph.find_node(a) == nullptr);
    assert(graph.find_port(a_in1) == nullptr);
    assert(graph.find_link(link1) == nullptr);

    wng::Graph port_graph;
    wng::NodeId c;
    wng::NodeId d;
    assert(port_graph.create_node(wng::NodeDesc{}, &c) == wng::Result::Ok);
    assert(port_graph.create_node(wng::NodeDesc{}, &d) == wng::Result::Ok);
    wng::PortId c_out;
    wng::PortId d_in;
    assert(port_graph.add_port(c, output, &c_out) == wng::Result::Ok);
    assert(port_graph.add_port(d, input, &d_in) == wng::Result::Ok);
    wng::LinkId linked;
    assert(port_graph.create_link(c_out, d_in, &linked) == wng::Result::Ok);
    assert(port_graph.remove_port(d_in, nullptr) == wng::Result::Ok);
    assert(port_graph.find_port(d_in) == nullptr);
    assert(port_graph.find_link(linked) == nullptr);

    return 0;
}
