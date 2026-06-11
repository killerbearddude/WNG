// Implements atomic redo application for command records and batches.
// Redo consumes recorded graph effects; it does not own history, undo state,
// editor selection, command replay, or schema policy.

#include <new>
#include <vector>

#include <wng/graph_redo.hpp>

#include <wng/graph_restore.hpp>
#include <wng/serialization.hpp>
#include <wng/serialization_dto.hpp>

namespace
{
    wng::GraphRedoResult redo_failure(wng::Result result)
    {
        wng::GraphRedoResult redo;
        redo.result = result;
        return redo;
    }

    wng::Result make_working_copy(const wng::Graph& graph, wng::Graph* out_graph)
    {
        if (out_graph == nullptr) {
            return wng::Result::InvalidArgument;
        }

        wng::GraphDto dto;
        const wng::Result export_result = wng::export_graph(graph, &dto);
        if (export_result != wng::Result::Ok) {
            return export_result;
        }

        return wng::import_graph(dto, out_graph);
    }

    wng::GraphObjectSnapshot created_snapshot_from_record(
        const wng::GraphCommandRecord& record)
    {
        wng::GraphObjectSnapshot snapshot;
        snapshot.nodes = record.created_nodes;
        snapshot.ports = record.created_ports;
        snapshot.links = record.created_links;
        return snapshot;
    }

    wng::Result remove_removed_links(
        wng::Graph& graph,
        const wng::GraphCommandRecord& record)
    {
        for (std::vector<wng::Link>::const_iterator it = record.removed_links.begin();
             it != record.removed_links.end();
             ++it) {
            if (graph.find_link(it->id) == nullptr) {
                return wng::Result::NotFound;
            }

            wng::GraphMutationSummary summary;
            const wng::Result result = graph.destroy_link(it->id, &summary);
            if (result != wng::Result::Ok) {
                return result;
            }
        }

        return wng::Result::Ok;
    }

    wng::Result remove_removed_ports(
        wng::Graph& graph,
        const wng::GraphCommandRecord& record)
    {
        for (std::vector<wng::Port>::const_iterator it = record.removed_ports.begin();
             it != record.removed_ports.end();
             ++it) {
            if (graph.find_port(it->id) == nullptr) {
                return wng::Result::NotFound;
            }

            wng::GraphMutationSummary summary;
            const wng::Result result = graph.remove_port(it->id, &summary);
            if (result != wng::Result::Ok) {
                return result;
            }
        }

        return wng::Result::Ok;
    }

    wng::Result remove_removed_nodes(
        wng::Graph& graph,
        const wng::GraphCommandRecord& record)
    {
        for (std::vector<wng::Node>::const_iterator it = record.removed_nodes.begin();
             it != record.removed_nodes.end();
             ++it) {
            if (graph.find_node(it->id) == nullptr) {
                return wng::Result::NotFound;
            }

            wng::GraphMutationSummary summary;
            const wng::Result result = graph.destroy_node(it->id, &summary);
            if (result != wng::Result::Ok) {
                return result;
            }
        }

        return wng::Result::Ok;
    }

    wng::Result remove_removed_objects(
        wng::Graph& graph,
        const wng::GraphCommandRecord& record)
    {
        // Redo is the sibling of undo but applies the opposite mapping: removed
        // snapshots are removed again and created snapshots are restored. Links
        // are removed before ports and nodes to keep dependency cleanup explicit.
        const wng::Result link_result = remove_removed_links(graph, record);
        if (link_result != wng::Result::Ok) {
            return link_result;
        }

        const wng::Result port_result = remove_removed_ports(graph, record);
        if (port_result != wng::Result::Ok) {
            return port_result;
        }

        return remove_removed_nodes(graph, record);
    }

    wng::Result restore_created_objects(
        wng::Graph& graph,
        const wng::GraphCommandRecord& record)
    {
        const wng::GraphObjectSnapshot snapshot = created_snapshot_from_record(record);
        const wng::GraphRestoreResult restore = wng::restore_graph_objects(graph, snapshot);
        return restore.result;
    }

    wng::Result redo_record_in_place(
        wng::Graph& graph,
        const wng::GraphCommandRecord& record)
    {
        if (record.result != wng::Result::Ok) {
            return wng::Result::InvalidArgument;
        }

        // Schema policy is intentionally not re-run during redo. Command records
        // describe graph effects that already happened; redo reapplies those
        // effects even if schemas have changed since the original command.
        const wng::Result remove_result = remove_removed_objects(graph, record);
        if (remove_result != wng::Result::Ok) {
            return remove_result;
        }

        return restore_created_objects(graph, record);
    }
}

namespace wng
{
    bool GraphRedoResult::success() const
    {
        return result == Result::Ok;
    }

    GraphRedoResult redo_command(
        Graph& graph,
        const GraphCommandRecord& record)
    {
        try {
            Graph working;
            const Result copy_result = make_working_copy(graph, &working);
            if (copy_result != Result::Ok) {
                return redo_failure(copy_result);
            }

            // Redo uses a DTO working copy for caller-visible atomicity. The
            // caller's graph is replaced only after every recorded graph effect
            // has been reapplied successfully.
            const Result redo_result = redo_record_in_place(working, record);
            if (redo_result != Result::Ok) {
                return redo_failure(redo_result);
            }

            GraphRedoResult result;
            result.result = Result::Ok;
            result.applied_records.push_back(record);
            graph = working;
            return result;
        } catch (const std::bad_alloc&) {
            return redo_failure(Result::AllocationFailure);
        }
    }

    GraphRedoResult redo_command_batch(
        Graph& graph,
        const GraphCommandBatch& batch)
    {
        try {
            if (batch.result != Result::Ok) {
                return redo_failure(Result::InvalidArgument);
            }

            Graph working;
            const Result copy_result = make_working_copy(graph, &working);
            if (copy_result != Result::Ok) {
                return redo_failure(copy_result);
            }

            GraphRedoResult result;
            result.result = Result::Ok;

            // Batches are redone in original command order because later records
            // may depend on objects restored by earlier records. Undo uses reverse
            // order; redo intentionally preserves the original dependency order.
            for (std::vector<GraphCommandRecord>::const_iterator it = batch.records.begin();
                 it != batch.records.end();
                 ++it) {
                const Result redo_result = redo_record_in_place(working, *it);
                if (redo_result != Result::Ok) {
                    return redo_failure(redo_result);
                }
                result.applied_records.push_back(*it);
            }

            graph = working;
            return result;
        } catch (const std::bad_alloc&) {
            return redo_failure(Result::AllocationFailure);
        }
    }
}
