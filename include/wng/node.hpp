#pragma once

#include <string>
#include <vector>

#include <wng/ids.hpp>
#include <wng/math.hpp>

namespace wng
{
    struct Node {
        NodeId id;
        std::string type;
        std::string title;
        Vec2 position;
        Vec2 size;
        std::vector<PortId> inputs;
        std::vector<PortId> outputs;
        bool visible = true;
        bool enabled = true;
    };

    struct NodeDesc {
        std::string type;
        std::string title;
        Vec2 position {};
        Vec2 size {};
        bool visible = true;
        bool enabled = true;
    };
}
