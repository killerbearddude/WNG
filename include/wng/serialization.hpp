// In-memory graph serialization conversion APIs for WNG-0.2.
// These APIs do not perform file I/O or define an on-disk format.

#pragma once

#include <wng/graph.hpp>
#include <wng/result.hpp>
#include <wng/serialization_dto.hpp>

namespace wng
{
    Result export_graph(const Graph& graph, GraphDto* out_graph);
    Result import_graph(const GraphDto& dto, Graph* out_graph);
}
