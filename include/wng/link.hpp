#pragma once

#include <wng/ids.hpp>

namespace wng
{
    struct Link {
        LinkId id;
        PortId from;
        PortId to;
        bool visible = true;
        bool enabled = true;
    };
}
