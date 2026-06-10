#include <cassert>
#include <string>

#include <wng/serialization_dto.hpp>

int main()
{
    wng::GraphDto dto;

    assert(dto.version.major == 0);
    assert(dto.version.minor == 2);
    assert(dto.version.patch == 0);
    assert(dto.nodes.empty());
    assert(dto.ports.empty());
    assert(dto.links.empty());

    wng::NodeDto node;
    node.id = wng::NodeId { 1 };
    node.type = "constant.number";
    node.title = "Source";
    node.position = wng::Vec2 { 10.0f, 20.0f };
    node.size = wng::Vec2 { 100.0f, 50.0f };
    node.outputs.push_back(wng::PortId { 1 });

    wng::PortDto output;
    output.id = wng::PortId { 1 };
    output.node = node.id;
    output.kind = wng::PortKind::Output;
    output.name = "value";
    output.type = "number";

    wng::NodeDto sink;
    sink.id = wng::NodeId { 2 };
    sink.type = "debug.print";
    sink.title = "Sink";
    sink.position = wng::Vec2 { 240.0f, 20.0f };
    sink.size = wng::Vec2 { 100.0f, 50.0f };
    sink.inputs.push_back(wng::PortId { 2 });

    wng::PortDto input;
    input.id = wng::PortId { 2 };
    input.node = sink.id;
    input.kind = wng::PortKind::Input;
    input.name = "value";
    input.type = "number";

    wng::LinkDto link;
    link.id = wng::LinkId { 1 };
    link.from = output.id;
    link.to = input.id;

    dto.nodes.push_back(node);
    dto.nodes.push_back(sink);
    dto.ports.push_back(output);
    dto.ports.push_back(input);
    dto.links.push_back(link);

    assert(dto.nodes.size() == 2);
    assert(dto.ports.size() == 2);
    assert(dto.links.size() == 1);

    assert(dto.nodes[0].id == wng::NodeId { 1 });
    assert(dto.nodes[0].type == "constant.number");
    assert(dto.nodes[0].outputs[0] == wng::PortId { 1 });
    assert(dto.nodes[1].inputs[0] == wng::PortId { 2 });
    assert(dto.nodes[1].type == "debug.print");

    assert(dto.ports[0].kind == wng::PortKind::Output);
    assert(dto.ports[1].kind == wng::PortKind::Input);

    assert(dto.links[0].from == wng::PortId { 1 });
    assert(dto.links[0].to == wng::PortId { 2 });

    return 0;
}
