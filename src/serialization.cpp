#include <new>
#include <utility>

#include <wng/serialization.hpp>

namespace wng
{
    Result export_graph(const Graph& graph, GraphDto* out_graph)
    {
        if (out_graph == nullptr) {
            return Result::InvalidArgument;
        }

        try {
            GraphDto exported;
            exported.version = GraphDtoVersion {};

            exported.nodes.reserve(graph.nodes().size());
            exported.ports.reserve(graph.ports().size());
            exported.links.reserve(graph.links().size());

            for (const Node& node : graph.nodes()) {
                NodeDto dto;
                dto.id = node.id;
                dto.title = node.title;
                dto.position = node.position;
                dto.size = node.size;
                dto.inputs = node.inputs;
                dto.outputs = node.outputs;
                dto.visible = node.visible;
                dto.enabled = node.enabled;
                exported.nodes.push_back(std::move(dto));
            }

            for (const Port& port : graph.ports()) {
                PortDto dto;
                dto.id = port.id;
                dto.node = port.node;
                dto.kind = port.kind;
                dto.name = port.name;
                dto.type = port.type;
                dto.visible = port.visible;
                dto.enabled = port.enabled;
                exported.ports.push_back(std::move(dto));
            }

            for (const Link& link : graph.links()) {
                LinkDto dto;
                dto.id = link.id;
                dto.from = link.from;
                dto.to = link.to;
                dto.visible = link.visible;
                dto.enabled = link.enabled;
                exported.links.push_back(dto);
            }

            out_graph->version = exported.version;
            out_graph->nodes.swap(exported.nodes);
            out_graph->ports.swap(exported.ports);
            out_graph->links.swap(exported.links);

            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }
}
