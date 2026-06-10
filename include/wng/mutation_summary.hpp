#pragma once

#include <vector>

#include <wng/ids.hpp>

namespace wng
{
    struct GraphMutationSummary {
        std::vector<NodeId> removed_nodes;
        std::vector<PortId> removed_ports;
        std::vector<LinkId> removed_links;
    };
}
