// Implements in-memory graph snapshot capture and restore for WNG.
// Snapshots are DTO-backed graph-core values; they are not save files, JSON
// documents, editor-state captures, or persistence policy objects.

#include <new>

#include <wng/graph_snapshot.hpp>

#include <wng/graph.hpp>
#include <wng/graph_validation.hpp>
#include <wng/serialization.hpp>

namespace
{
    wng::Result first_error_result(const wng::ValidationReport& report)
    {
        for (const wng::ValidationIssue& issue : report.issues) {
            if (issue.severity == wng::ValidationSeverity::Error) {
                return issue.result;
            }
        }

        return wng::Result::Ok;
    }

    wng::GraphSnapshotResult snapshot_failure(wng::Result result)
    {
        wng::GraphSnapshotResult snapshot_result;
        snapshot_result.result = result;
        return snapshot_result;
    }
}

namespace wng
{
    bool GraphSnapshot::empty() const
    {
        return graph.nodes.empty() &&
            graph.ports.empty() &&
            graph.links.empty();
    }

    bool GraphSnapshotResult::success() const
    {
        return result == Result::Ok;
    }

    GraphSnapshotResult capture_graph_snapshot(
        const Graph& graph)
    {
        try {
            // Snapshot capture validates first so callers do not accidentally
            // promote a structurally broken imported graph into reusable state.
            const ValidationReport report = validate_graph(graph);
            if (report.has_errors()) {
                return snapshot_failure(first_error_result(report));
            }

            GraphDto dto;
            const Result export_result = export_graph(graph, &dto);
            if (export_result != Result::Ok) {
                return snapshot_failure(export_result);
            }

            GraphSnapshotResult snapshot_result;
            snapshot_result.snapshot.graph = dto;
            return snapshot_result;
        } catch (const std::bad_alloc&) {
            return snapshot_failure(Result::AllocationFailure);
        }
    }

    Result restore_graph_snapshot(
        Graph& graph,
        const GraphSnapshot& snapshot)
    {
        try {
            Graph replacement;
            const Result import_result = import_graph(snapshot.graph, &replacement);
            if (import_result != Result::Ok) {
                return import_result;
            }

            // Restore imports into a temporary graph and validates it before
            // assignment. The target graph is replaced only after all checks pass,
            // preserving caller-visible atomicity on failure.
            const ValidationReport report = validate_graph(replacement);
            if (report.has_errors()) {
                return first_error_result(report);
            }

            graph = replacement;
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }
}
