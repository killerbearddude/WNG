#pragma once

#include <string>

#include <wng/ids.hpp>

namespace wng
{
    enum class PortKind {
        Input,
        Output
    };

    struct Port {
        PortId id;
        NodeId node;
        PortKind kind;
        std::string name;
        std::string type;
        bool visible = true;
        bool enabled = true;
    };

    struct PortDesc {
        PortKind kind = PortKind::Input;
        std::string name;
        std::string type;
        bool visible = true;
        bool enabled = true;
    };
}
