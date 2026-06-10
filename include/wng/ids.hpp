#pragma once

#include <cstdint>

namespace wng
{
    struct NodeId {
        std::uint32_t value = 0;
    };

    struct PortId {
        std::uint32_t value = 0;
    };

    struct LinkId {
        std::uint32_t value = 0;
    };

    inline bool operator==(NodeId a, NodeId b) { return a.value == b.value; }
    inline bool operator!=(NodeId a, NodeId b) { return !(a == b); }

    inline bool operator==(PortId a, PortId b) { return a.value == b.value; }
    inline bool operator!=(PortId a, PortId b) { return !(a == b); }

    inline bool operator==(LinkId a, LinkId b) { return a.value == b.value; }
    inline bool operator!=(LinkId a, LinkId b) { return !(a == b); }
}
