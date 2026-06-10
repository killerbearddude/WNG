#include <cassert>
#include <cmath>
#include <limits>
#include <string>

#include <wng/serialization.hpp>

namespace
{
    wng::NodeDto make_node(wng::NodeId id, const char* title)
    {
        wng::NodeDto node;
        node.id = id;
        node.title = title;
        node.position = wng::Vec2 { static_cast<float>(id.value), static_cast<float>(id.value + 1U) };
        node.size = wng::Vec2 { 100.0f, 50.0f };
        return node;
    }

    wng::PortDto make_port(wng::PortId id, wng::NodeId node, wng::PortKind kind, const char* type = "number")
    {
        wng::PortDto port;
        port.id = id;
        port.node = node;
        port.kind = kind;
        port.name = kind == wng::PortKind::Input ? "in" : "out";
        port.type = type;
        return port;
    }

    wng::LinkDto make_link(wng::LinkId id, wng::PortId from, wng::PortId to)
    {
        wng::LinkDto link;
        link.id = id;
        link.from = from;
        link.to = to;
        return link;
    }

    wng::GraphDto make_valid_graph_dto()
    {
        wng::GraphDto dto;

        wng::NodeDto source = make_node(wng::NodeId { 1 }, "Source");
        source.visible = false;
        source.enabled = true;
        source.outputs.push_back(wng::PortId { 1 });

        wng::NodeDto sink = make_node(wng::NodeId { 2 }, "Sink");
        sink.position = wng::Vec2 { 240.0f, 20.0f };
        sink.size = wng::Vec2 { 120.0f, 60.0f };
        sink.inputs.push_back(wng::PortId { 2 });

        wng::PortDto output = make_port(wng::PortId { 1 }, source.id, wng::PortKind::Output);
        output.name = "value";
        output.visible = true;
        output.enabled = true;

        wng::PortDto input = make_port(wng::PortId { 2 }, sink.id, wng::PortKind::Input);
        input.name = "value";
        input.visible = false;
        input.enabled = true;

        wng::LinkDto link = make_link(wng::LinkId { 1 }, output.id, input.id);
        link.visible = false;
        link.enabled = true;

        dto.nodes.push_back(source);
        dto.nodes.push_back(sink);
        dto.ports.push_back(output);
        dto.ports.push_back(input);
        dto.links.push_back(link);
        return dto;
    }

    void assert_imported_valid_graph(const wng::Graph& graph)
    {
        assert(graph.nodes().size() == 2U);
        assert(graph.ports().size() == 2U);
        assert(graph.links().size() == 1U);

        assert(graph.nodes()[0].id == wng::NodeId { 1 });
        assert(graph.nodes()[0].title == "Source");
        assert(graph.nodes()[0].position.x == 1.0f);
        assert(graph.nodes()[0].position.y == 2.0f);
        assert(graph.nodes()[0].size.x == 100.0f);
        assert(graph.nodes()[0].size.y == 50.0f);
        assert(graph.nodes()[0].outputs.size() == 1U);
        assert(graph.nodes()[0].outputs[0] == wng::PortId { 1 });
        assert(graph.nodes()[0].visible == false);
        assert(graph.nodes()[0].enabled == true);

        assert(graph.nodes()[1].id == wng::NodeId { 2 });
        assert(graph.nodes()[1].title == "Sink");
        assert(graph.nodes()[1].position.x == 240.0f);
        assert(graph.nodes()[1].position.y == 20.0f);
        assert(graph.nodes()[1].size.x == 120.0f);
        assert(graph.nodes()[1].size.y == 60.0f);
        assert(graph.nodes()[1].inputs.size() == 1U);
        assert(graph.nodes()[1].inputs[0] == wng::PortId { 2 });

        assert(graph.ports()[0].id == wng::PortId { 1 });
        assert(graph.ports()[0].node == wng::NodeId { 1 });
        assert(graph.ports()[0].kind == wng::PortKind::Output);
        assert(graph.ports()[0].name == "value");
        assert(graph.ports()[0].type == "number");
        assert(graph.ports()[0].visible == true);
        assert(graph.ports()[0].enabled == true);

        assert(graph.ports()[1].id == wng::PortId { 2 });
        assert(graph.ports()[1].node == wng::NodeId { 2 });
        assert(graph.ports()[1].kind == wng::PortKind::Input);
        assert(graph.ports()[1].visible == false);
        assert(graph.ports()[1].enabled == true);

        assert(graph.links()[0].id == wng::LinkId { 1 });
        assert(graph.links()[0].from == wng::PortId { 1 });
        assert(graph.links()[0].to == wng::PortId { 2 });
        assert(graph.links()[0].visible == false);
        assert(graph.links()[0].enabled == true);
    }

    void assert_dto_equal(const wng::GraphDto& a, const wng::GraphDto& b)
    {
        assert(a.version.major == b.version.major);
        assert(a.version.minor == b.version.minor);
        assert(a.version.patch == b.version.patch);
        assert(a.nodes.size() == b.nodes.size());
        assert(a.ports.size() == b.ports.size());
        assert(a.links.size() == b.links.size());

        for (std::vector<wng::NodeDto>::size_type i = 0; i < a.nodes.size(); ++i) {
            assert(a.nodes[i].id == b.nodes[i].id);
            assert(a.nodes[i].title == b.nodes[i].title);
            assert(a.nodes[i].position.x == b.nodes[i].position.x);
            assert(a.nodes[i].position.y == b.nodes[i].position.y);
            assert(a.nodes[i].size.x == b.nodes[i].size.x);
            assert(a.nodes[i].size.y == b.nodes[i].size.y);
            assert(a.nodes[i].inputs == b.nodes[i].inputs);
            assert(a.nodes[i].outputs == b.nodes[i].outputs);
            assert(a.nodes[i].visible == b.nodes[i].visible);
            assert(a.nodes[i].enabled == b.nodes[i].enabled);
        }

        for (std::vector<wng::PortDto>::size_type i = 0; i < a.ports.size(); ++i) {
            assert(a.ports[i].id == b.ports[i].id);
            assert(a.ports[i].node == b.ports[i].node);
            assert(a.ports[i].kind == b.ports[i].kind);
            assert(a.ports[i].name == b.ports[i].name);
            assert(a.ports[i].type == b.ports[i].type);
            assert(a.ports[i].visible == b.ports[i].visible);
            assert(a.ports[i].enabled == b.ports[i].enabled);
        }

        for (std::vector<wng::LinkDto>::size_type i = 0; i < a.links.size(); ++i) {
            assert(a.links[i].id == b.links[i].id);
            assert(a.links[i].from == b.links[i].from);
            assert(a.links[i].to == b.links[i].to);
            assert(a.links[i].visible == b.links[i].visible);
            assert(a.links[i].enabled == b.links[i].enabled);
        }
    }

    void expect_import_result(wng::GraphDto dto, wng::Result expected)
    {
        wng::Graph graph;
        assert(wng::import_graph(dto, &graph) == expected);
    }
}

int main()
{
    {
        wng::GraphDto dto;
        wng::Graph graph;
        assert(wng::import_graph(dto, &graph) == wng::Result::Ok);
        assert(graph.nodes().empty());
        assert(graph.ports().empty());
        assert(graph.links().empty());

        wng::NodeId node;
        assert(graph.create_node(wng::NodeDesc {}, &node) == wng::Result::Ok);
        assert(node == wng::NodeId { 1 });
    }

    {
        wng::Graph graph;
        assert(wng::import_graph(make_valid_graph_dto(), &graph) == wng::Result::Ok);
        assert_imported_valid_graph(graph);
    }

    {
        wng::GraphDto dto;
        wng::NodeDto left = make_node(wng::NodeId { 10 }, "Left");
        left.outputs.push_back(wng::PortId { 7 });
        wng::NodeDto right = make_node(wng::NodeId { 50 }, "Right");
        right.inputs.push_back(wng::PortId { 30 });
        dto.nodes.push_back(left);
        dto.nodes.push_back(right);
        dto.ports.push_back(make_port(wng::PortId { 7 }, left.id, wng::PortKind::Output));
        dto.ports.push_back(make_port(wng::PortId { 30 }, right.id, wng::PortKind::Input));
        dto.links.push_back(make_link(wng::LinkId { 12 }, wng::PortId { 7 }, wng::PortId { 30 }));

        wng::Graph graph;
        assert(wng::import_graph(dto, &graph) == wng::Result::Ok);

        wng::NodeId next_node;
        assert(graph.create_node(wng::NodeDesc {}, &next_node) == wng::Result::Ok);
        assert(next_node == wng::NodeId { 51 });

        wng::PortDesc input_desc;
        input_desc.kind = wng::PortKind::Input;
        input_desc.type = "number";
        wng::PortId next_port;
        assert(graph.add_port(next_node, input_desc, &next_port) == wng::Result::Ok);
        assert(next_port == wng::PortId { 31 });

        wng::LinkId next_link;
        assert(graph.create_link(wng::PortId { 7 }, next_port, &next_link) == wng::Result::Ok);
        assert(next_link == wng::LinkId { 13 });
    }

    {
        wng::GraphDto dto;
        wng::Graph graph;
        assert(wng::import_graph(dto, nullptr) == wng::Result::InvalidArgument);

        dto.version.minor = 99;
        assert(wng::import_graph(dto, &graph) == wng::Result::InvalidArgument);
    }

    {
        wng::Graph destination;
        wng::NodeDesc original_desc;
        original_desc.title = "Original";
        wng::NodeId original;
        assert(destination.create_node(original_desc, &original) == wng::Result::Ok);

        wng::GraphDto invalid;
        invalid.nodes.push_back(make_node(wng::NodeId { 1 }, "A"));
        invalid.nodes.push_back(make_node(wng::NodeId { 1 }, "B"));
        assert(wng::import_graph(invalid, &destination) == wng::Result::AlreadyExists);

        assert(destination.nodes().size() == 1U);
        assert(destination.nodes()[0].id == original);
        assert(destination.nodes()[0].title == "Original");
        assert(destination.ports().empty());
        assert(destination.links().empty());
    }

    {
        wng::GraphDto dto = make_valid_graph_dto();
        dto.nodes[0].id = wng::NodeId {};
        expect_import_result(dto, wng::Result::InvalidArgument);

        dto = make_valid_graph_dto();
        dto.ports[0].id = wng::PortId {};
        expect_import_result(dto, wng::Result::InvalidArgument);

        dto = make_valid_graph_dto();
        dto.links[0].id = wng::LinkId {};
        expect_import_result(dto, wng::Result::InvalidArgument);
    }

    {
        wng::GraphDto dto = make_valid_graph_dto();
        dto.nodes[1].id = dto.nodes[0].id;
        expect_import_result(dto, wng::Result::AlreadyExists);

        dto = make_valid_graph_dto();
        dto.ports[1].id = dto.ports[0].id;
        expect_import_result(dto, wng::Result::AlreadyExists);

        dto = make_valid_graph_dto();
        dto.links.push_back(make_link(dto.links[0].id, dto.links[0].from, dto.links[0].to));
        expect_import_result(dto, wng::Result::AlreadyExists);
    }

    {
        wng::GraphDto dto = make_valid_graph_dto();
        dto.ports[0].node = wng::NodeId { 99 };
        expect_import_result(dto, wng::Result::NotFound);

        dto = make_valid_graph_dto();
        dto.links[0].from = wng::PortId { 99 };
        expect_import_result(dto, wng::Result::NotFound);

        dto = make_valid_graph_dto();
        dto.links[0].to = wng::PortId { 99 };
        expect_import_result(dto, wng::Result::NotFound);
    }

    {
        wng::GraphDto dto = make_valid_graph_dto();
        dto.ports[0].kind = static_cast<wng::PortKind>(99);
        expect_import_result(dto, wng::Result::InvalidArgument);

        dto = make_valid_graph_dto();
        dto.nodes[0].inputs.push_back(wng::PortId { 1 });
        expect_import_result(dto, wng::Result::InvalidConnection);

        dto = make_valid_graph_dto();
        dto.nodes[1].inputs[0] = wng::PortId { 1 };
        expect_import_result(dto, wng::Result::InvalidConnection);

        dto = make_valid_graph_dto();
        dto.nodes[0].outputs[0] = wng::PortId { 2 };
        expect_import_result(dto, wng::Result::InvalidConnection);
    }

    {
        wng::GraphDto dto = make_valid_graph_dto();
        dto.links[0].to = dto.links[0].from;
        expect_import_result(dto, wng::Result::InvalidConnection);

        dto = make_valid_graph_dto();
        dto.links[0].from = wng::PortId { 2 };
        dto.links[0].to = wng::PortId { 1 };
        expect_import_result(dto, wng::Result::InvalidConnection);

        dto = make_valid_graph_dto();
        dto.nodes[0].inputs.push_back(wng::PortId { 3 });
        dto.ports.push_back(make_port(wng::PortId { 3 }, wng::NodeId { 1 }, wng::PortKind::Input));
        dto.links[0].to = wng::PortId { 3 };
        expect_import_result(dto, wng::Result::InvalidConnection);

        dto = make_valid_graph_dto();
        dto.links.push_back(make_link(wng::LinkId { 2 }, dto.links[0].from, dto.links[0].to));
        expect_import_result(dto, wng::Result::AlreadyExists);

        dto = make_valid_graph_dto();
        wng::NodeDto extra = make_node(wng::NodeId { 3 }, "Extra");
        extra.outputs.push_back(wng::PortId { 3 });
        dto.nodes.push_back(extra);
        dto.ports.push_back(make_port(wng::PortId { 3 }, extra.id, wng::PortKind::Output));
        dto.links.push_back(make_link(wng::LinkId { 2 }, wng::PortId { 3 }, wng::PortId { 2 }));
        expect_import_result(dto, wng::Result::InvalidConnection);

        dto = make_valid_graph_dto();
        dto.ports[1].type = "string";
        expect_import_result(dto, wng::Result::InvalidConnection);
    }

    {
        wng::GraphDto dto = make_valid_graph_dto();
        dto.nodes[0].enabled = false;
        dto.ports[0].enabled = false;
        dto.links[0].enabled = false;

        wng::Graph graph;
        assert(wng::import_graph(dto, &graph) == wng::Result::Ok);
        assert(graph.nodes()[0].enabled == false);
        assert(graph.ports()[0].enabled == false);
        assert(graph.links()[0].enabled == false);
        assert(graph.links().size() == 1U);
    }

    {
        wng::GraphDto dto = make_valid_graph_dto();
        dto.nodes[0].position.x = std::nanf("");
        expect_import_result(dto, wng::Result::InvalidArgument);

        dto = make_valid_graph_dto();
        dto.nodes[0].size.y = std::numeric_limits<float>::infinity();
        expect_import_result(dto, wng::Result::InvalidArgument);

        dto = make_valid_graph_dto();
        dto.nodes[0].size.x = -1.0f;
        expect_import_result(dto, wng::Result::InvalidArgument);
    }

    {
        wng::Graph source;
        wng::NodeId left;
        wng::NodeId right;
        assert(source.create_node(wng::NodeDesc {}, &left) == wng::Result::Ok);
        assert(source.create_node(wng::NodeDesc {}, &right) == wng::Result::Ok);

        wng::PortDesc output;
        output.kind = wng::PortKind::Output;
        output.type = "number";
        wng::PortId out;
        assert(source.add_port(left, output, &out) == wng::Result::Ok);

        wng::PortDesc input;
        input.kind = wng::PortKind::Input;
        input.type = "number";
        wng::PortId in;
        assert(source.add_port(right, input, &in) == wng::Result::Ok);

        wng::LinkId link;
        assert(source.create_link(out, in, &link) == wng::Result::Ok);

        wng::GraphDto exported;
        assert(wng::export_graph(source, &exported) == wng::Result::Ok);

        wng::Graph copy;
        assert(wng::import_graph(exported, &copy) == wng::Result::Ok);

        wng::GraphDto round_trip;
        assert(wng::export_graph(copy, &round_trip) == wng::Result::Ok);
        assert_dto_equal(exported, round_trip);
    }

    return 0;
}
