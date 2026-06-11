// Captures and restores complete WNG graph state as an in-memory value.
// This layer wraps DTO import/export for snapshot use cases without introducing
// file I/O, JSON, editor state, or persistence policy.

#pragma once

#include <cstdint>

#include <wng/result.hpp>
#include <wng/serialization_dto.hpp>

namespace wng
{
    class Graph;

    // Names an in-memory graph snapshot built on the existing GraphDto model.
    // This wrapper is not a file format and carries no persistence policy.
    struct GraphSnapshot {
        // Version for the snapshot wrapper itself. This is not a file format
        // version and should not be treated as persistence metadata.
        std::uint32_t snapshot_version = 1;

        // Complete graph DTO captured from Graph. The DTO remains the structural
        // representation; GraphSnapshot only names the in-memory use case.
        GraphDto graph;

        bool empty() const;
    };

    // Result wrapper for snapshot capture. A failed result leaves snapshot as its
    // default empty value so callers never receive a partially valid capture.
    struct GraphSnapshotResult {
        Result result = Result::Ok;
        GraphSnapshot snapshot;

        bool success() const;
    };

    // Captures the current graph into an in-memory snapshot.
    // The graph is validated before export so invalid imported states are not
    // accidentally promoted into reusable snapshots.
    GraphSnapshotResult capture_graph_snapshot(
        const Graph& graph);

    // Replaces the target graph with the graph stored in the snapshot.
    // The operation is atomic from the caller's perspective: on failure, the
    // target graph remains unchanged.
    Result restore_graph_snapshot(
        Graph& graph,
        const GraphSnapshot& snapshot);
}
