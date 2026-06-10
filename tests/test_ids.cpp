#include <cassert>
#include <cstdint>
#include <type_traits>

#include <wng/wng.hpp>

int main()
{
    static_assert(__cplusplus >= 201703L, "WNG tests require C++17 or newer language mode");
    static_assert(std::is_same<decltype(wng::NodeId{}.value), std::uint32_t>::value, "NodeId uses uint32_t");
    static_assert(std::is_same<decltype(wng::PortId{}.value), std::uint32_t>::value, "PortId uses uint32_t");
    static_assert(std::is_same<decltype(wng::LinkId{}.value), std::uint32_t>::value, "LinkId uses uint32_t");

    assert(wng::NodeId{}.value == 0U);
    assert(wng::PortId{}.value == 0U);
    assert(wng::LinkId{}.value == 0U);

    assert(wng::NodeId{1} == wng::NodeId{1});
    assert(wng::NodeId{1} != wng::NodeId{2});
    assert(wng::PortId{1} == wng::PortId{1});
    assert(wng::PortId{1} != wng::PortId{2});
    assert(wng::LinkId{1} == wng::LinkId{1});
    assert(wng::LinkId{1} != wng::LinkId{2});

    wng::Graph graph;
    assert(graph.find_node(wng::NodeId{}) == nullptr);
    assert(graph.find_port(wng::PortId{}) == nullptr);
    assert(graph.find_link(wng::LinkId{}) == nullptr);
    assert(graph.validate_connection(wng::PortId{}, wng::PortId{1}).result == wng::Result::InvalidArgument);

    return 0;
}
