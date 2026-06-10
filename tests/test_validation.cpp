#include <cassert>

#include <wng/wng.hpp>

int main()
{
    wng::Graph graph;
    wng::NodeId node_a;
    wng::NodeId node_b;
    assert(graph.create_node(wng::NodeDesc{}, &node_a) == wng::Result::Ok);
    assert(graph.create_node(wng::NodeDesc{}, &node_b) == wng::Result::Ok);

    wng::PortDesc in_desc;
    in_desc.kind = wng::PortKind::Input;
    wng::PortDesc out_desc;
    out_desc.kind = wng::PortKind::Output;

    wng::PortId a_in;
    wng::PortId a_out;
    wng::PortId b_in;
    wng::PortId b_out;
    assert(graph.add_port(node_a, in_desc, &a_in) == wng::Result::Ok);
    assert(graph.add_port(node_a, out_desc, &a_out) == wng::Result::Ok);
    assert(graph.add_port(node_b, in_desc, &b_in) == wng::Result::Ok);
    assert(graph.add_port(node_b, out_desc, &b_out) == wng::Result::Ok);

    assert(graph.validate_connection(a_in, b_in).result == wng::Result::InvalidConnection);
    assert(graph.validate_connection(a_out, a_in).result == wng::Result::InvalidConnection);
    assert(graph.validate_connection(a_out, b_in).status == wng::ConnectionStatus::Allowed);

    wng::LinkId first;
    assert(graph.create_link(a_out, b_in, &first) == wng::Result::Ok);
    assert(graph.validate_connection(a_out, b_in).result == wng::Result::AlreadyExists);
    assert(graph.validate_connection(b_out, b_in).result == wng::Result::InvalidConnection);

    wng::Graph disabled_node_graph;
    wng::NodeDesc disabled_node;
    disabled_node.enabled = false;
    wng::NodeId disabled;
    wng::NodeId enabled;
    assert(disabled_node_graph.create_node(disabled_node, &disabled) == wng::Result::Ok);
    assert(disabled_node_graph.create_node(wng::NodeDesc{}, &enabled) == wng::Result::Ok);
    wng::PortId disabled_out;
    wng::PortId enabled_in;
    assert(disabled_node_graph.add_port(disabled, out_desc, &disabled_out) == wng::Result::Ok);
    assert(disabled_node_graph.add_port(enabled, in_desc, &enabled_in) == wng::Result::Ok);
    assert(disabled_node_graph.validate_connection(disabled_out, enabled_in).result == wng::Result::InvalidConnection);

    wng::Graph disabled_port_graph;
    wng::NodeId n1;
    wng::NodeId n2;
    assert(disabled_port_graph.create_node(wng::NodeDesc{}, &n1) == wng::Result::Ok);
    assert(disabled_port_graph.create_node(wng::NodeDesc{}, &n2) == wng::Result::Ok);
    wng::PortDesc disabled_output = out_desc;
    disabled_output.enabled = false;
    wng::PortId disabled_port;
    wng::PortId target;
    assert(disabled_port_graph.add_port(n1, disabled_output, &disabled_port) == wng::Result::Ok);
    assert(disabled_port_graph.add_port(n2, in_desc, &target) == wng::Result::Ok);
    assert(disabled_port_graph.validate_connection(disabled_port, target).result == wng::Result::InvalidConnection);

    return 0;
}
