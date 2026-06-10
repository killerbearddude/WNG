#include <wng/validation.hpp>

#include <wng/graph.hpp>

namespace wng
{
    ConnectionValidation validate_connection(const Graph& graph, PortId from, PortId to)
    {
        return graph.validate_connection(from, to);
    }
}
