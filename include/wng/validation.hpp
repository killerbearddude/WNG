#pragma once

#include <wng/ids.hpp>
#include <wng/result.hpp>

namespace wng
{
    class Graph;

    enum class ConnectionStatus {
        Allowed,
        Rejected
    };

    struct ConnectionValidation {
        ConnectionStatus status = ConnectionStatus::Rejected;
        Result result = Result::InvalidConnection;
    };

    ConnectionValidation validate_connection(const Graph& graph, PortId from, PortId to);
}
