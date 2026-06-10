#include <cassert>

#include <wng/wng.hpp>

namespace
{
    struct SimpleGraph {
        wng::Graph graph;
        wng::NodeId left;
        wng::NodeId right;
        wng::PortId out;
        wng::PortId in;
    };

    SimpleGraph make_simple_graph(const char* output_type = "float", const char* input_type = "float")
    {
        SimpleGraph sg;
        assert(sg.graph.create_node(wng::NodeDesc{}, &sg.left) == wng::Result::Ok);
        assert(sg.graph.create_node(wng::NodeDesc{}, &sg.right) == wng::Result::Ok);

        wng::PortDesc output;
        output.kind = wng::PortKind::Output;
        output.type = output_type;
        assert(sg.graph.add_port(sg.left, output, &sg.out) == wng::Result::Ok);

        wng::PortDesc input;
        input.kind = wng::PortKind::Input;
        input.type = input_type;
        assert(sg.graph.add_port(sg.right, input, &sg.in) == wng::Result::Ok);
        return sg;
    }
}

int main()
{
    SimpleGraph sg = make_simple_graph();
    wng::LinkId link;
    assert(sg.graph.create_link(sg.out, sg.in, nullptr) == wng::Result::InvalidArgument);
    assert(sg.graph.create_link(wng::PortId{}, sg.in, &link) == wng::Result::InvalidArgument);
    assert(sg.graph.create_link(wng::PortId{99}, sg.in, &link) == wng::Result::NotFound);

    assert(sg.graph.validate_connection(sg.out, sg.in).status == wng::ConnectionStatus::Allowed);
    assert(wng::validate_connection(sg.graph, sg.out, sg.in).status == wng::ConnectionStatus::Allowed);
    assert(sg.graph.create_link(sg.out, sg.in, &link) == wng::Result::Ok);
    assert(link.value == 1U);
    assert(sg.graph.links().size() == 1U);
    assert(sg.graph.find_link(link) != nullptr);
    assert(sg.graph.create_link(sg.out, sg.in, &link) == wng::Result::AlreadyExists);

    wng::GraphMutationSummary summary;
    assert(sg.graph.destroy_link(wng::LinkId{}, &summary) == wng::Result::InvalidArgument);
    assert(sg.graph.destroy_link(wng::LinkId{99}, &summary) == wng::Result::NotFound);
    assert(sg.graph.destroy_link(link, &summary) == wng::Result::Ok);
    assert(summary.removed_nodes.empty());
    assert(summary.removed_ports.empty());
    assert(summary.removed_links.size() == 1U);
    assert(summary.removed_links[0] == link);

    wng::LinkId next;
    assert(sg.graph.create_link(sg.out, sg.in, &next) == wng::Result::Ok);
    assert(next.value == 2U);

    SimpleGraph mismatch = make_simple_graph("float", "int");
    assert(mismatch.graph.create_link(mismatch.out, mismatch.in, &link) == wng::Result::InvalidConnection);

    SimpleGraph any_output = make_simple_graph("any", "int");
    assert(any_output.graph.create_link(any_output.out, any_output.in, &link) == wng::Result::Ok);

    SimpleGraph untyped = make_simple_graph("", "custom");
    assert(untyped.graph.create_link(untyped.out, untyped.in, &link) == wng::Result::Ok);

    return 0;
}
