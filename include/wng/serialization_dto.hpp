// In-memory graph serialization DTO declarations for WNG-0.2.
// These types are format-agnostic and do not perform file I/O.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <wng/ids.hpp>
#include <wng/math.hpp>
#include <wng/port.hpp>

namespace wng
{
    struct GraphDtoVersion {
        std::uint32_t major = 0;
        std::uint32_t minor = 2;
        std::uint32_t patch = 0;
    };

    struct NodeDto {
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

    struct PortDto {
        PortId id;
        NodeId node;
        PortKind kind = PortKind::Input;
        std::string name;
        std::string type;
        bool visible = true;
        bool enabled = true;
    };

    struct LinkDto {
        LinkId id;
        PortId from;
        PortId to;
        bool visible = true;
        bool enabled = true;
    };

    struct GraphDto {
        GraphDtoVersion version;
        std::vector<NodeDto> nodes;
        std::vector<PortDto> ports;
        std::vector<LinkDto> links;
    };
}
