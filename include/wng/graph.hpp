#pragma once

#include <cstdint>
#include <vector>

#include <wng/ids.hpp>
#include <wng/link.hpp>
#include <wng/mutation_summary.hpp>
#include <wng/node.hpp>
#include <wng/port.hpp>
#include <wng/result.hpp>
#include <wng/validation.hpp>

namespace wng
{
    struct GraphDto;
    class Graph;

    Result import_graph(const GraphDto& dto, Graph* out_graph);

    class Graph {
    public:
        Result create_node(const NodeDesc& desc, NodeId* out_id);
        Result destroy_node(NodeId id, GraphMutationSummary* out_summary);

        Result add_port(NodeId node, const PortDesc& desc, PortId* out_id);
        Result remove_port(PortId id, GraphMutationSummary* out_summary);

        Result create_link(PortId from, PortId to, LinkId* out_id);
        Result destroy_link(LinkId id, GraphMutationSummary* out_summary);

        ConnectionValidation validate_connection(PortId from, PortId to) const;

        Node* find_node(NodeId id);
        Port* find_port(PortId id);
        Link* find_link(LinkId id);

        const Node* find_node(NodeId id) const;
        const Port* find_port(PortId id) const;
        const Link* find_link(LinkId id) const;

        const std::vector<Node>& nodes() const;
        const std::vector<Port>& ports() const;
        const std::vector<Link>& links() const;

    private:
        friend Result import_graph(const GraphDto& dto, Graph* out_graph);

        std::vector<Node> nodes_;
        std::vector<Port> ports_;
        std::vector<Link> links_;
        std::uint32_t next_node_id_ = 1;
        std::uint32_t next_port_id_ = 1;
        std::uint32_t next_link_id_ = 1;
    };
}
