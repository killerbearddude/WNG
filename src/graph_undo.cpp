// Implements atomic undo application for command records and batches.
// Undo consumes recorded graph effects; it does not own history, redo state,
// editor selection, command replay, or schema policy.

#include <new>
#include <vector>

#include <wng/graph_undo.hpp>

#include <wng/graph_restore.hpp>
#include <wng/serialization.hpp>
#include <wng/serialization_dto.hpp>

namespace
{
    wng::GraphUndoResult undo_failure(wng::Result result)
    {
        wng::GraphUndoResult undo;
        undo.result = result;
        return undo;
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

    wng::GraphObjectSnapshot removed_snapshot_from_record(
        const wng::GraphCommandRecord& record)
    {
        wng::GraphObjectSnapshot snapshot;
        snapshot.nodes = record.removed_nodes;
        snapshot.ports = record.removed_ports;
        snapshot.links = record.removed_links;
        return snapshot;
    }

    wng::Result remove_created_links(
        wng::Graph& graph,
        const wng::GraphCommandRecord& record)
    {
        for (std::vector<wng::Link>::const_reverse_iterator it = record.created_links.rbegin();
             it != record.created_links.rend();
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

    wng::Result remove_created_ports(
        wng::Graph& graph,
        const wng::GraphCommandRecord& record)
    {
        for (std::vector<wng::Port>::const_reverse_iterator it = record.created_ports.rbegin();
             it != record.created_ports.rend();
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

    wng::Result remove_created_nodes(
        wng::Graph& graph,
        const wng::GraphCommandRecord& record)
    {
        for (std::vector<wng::Node>::const_reverse_iterator it = record.created_nodes.rbegin();
             it != record.created_nodes.rend();
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

    wng::Result remove_created_objects(
        wng::Graph& graph,
        const wng::GraphCommandRecord& record)
    {
        // Created objects are removed before removed snapshots are restored. This
        // prevents ID collisions when a future compound command records both sides
        // of a replacement-style mutation.
        const wng::Result link_result = remove_created_links(graph, record);
        if (link_result != wng::Result::Ok) {
            return link_result;
        }

        const wng::Result port_result = remove_created_ports(graph, record);
        if (port_result != wng::Result::Ok) {
            return port_result;
        }

        return remove_created_nodes(graph, record);
    }

    wng::Result restore_removed_objects(
        wng::Graph& graph,
        const wng::GraphCommandRecord& record)
    {
        const wng::GraphObjectSnapshot snapshot = removed_snapshot_from_record(record);
        if (snapshot.empty()) {
            return wng::Result::Ok;
        }

        const wng::GraphRestoreResult restore = wng::restore_graph_objects(graph, snapshot);
        return restore.result;
    }

    wng::Result undo_record_in_place(
        wng::Graph& graph,
        const wng::GraphCommandRecord& record)
    {
        if (record.result != wng::Result::Ok) {
            return wng::Result::InvalidArgument;
        }

        // Undo uses recorded graph effects, not command kind policy. Schema-aware
        // commands are undone from their created/removed snapshots so undo does
        // not fail merely because schema rules changed after recording.
        const wng::Result remove_result = remove_created_objects(graph, record);
        if (remove_result != wng::Result::Ok) {
            return remove_result;
        }

        return restore_removed_objects(graph, record);
    }
}

namespace wng
{
    bool GraphUndoResult::success() const
    {
        return result == Result::Ok;
    }

    GraphUndoResult undo_command(
        Graph& graph,
        const GraphCommandRecord& record)
    {
        try {
            if (record.result != Result::Ok) {
                return undo_failure(Result::InvalidArgument);
            }

            Graph working;
            const Result copy_result = make_working_copy(graph, &working);
            if (copy_result != Result::Ok) {
                return undo_failure(copy_result);
            }

            // All inverse mutations happen on a DTO-created working graph. The
            // caller's graph is replaced only after the full undo succeeds.
            const Result undo_result = undo_record_in_place(working, record);
            if (undo_result != Result::Ok) {
                return undo_failure(undo_result);
            }

            graph = working;

            GraphUndoResult result;
            result.result = Result::Ok;
            result.applied_records.push_back(record);
            return result;
        } catch (const std::bad_alloc&) {
            return undo_failure(Result::AllocationFailure);
        }
    }

    GraphUndoResult undo_command_batch(
        Graph& graph,
        const GraphCommandBatch& batch)
    {
        try {
            if (batch.result != Result::Ok) {
                return undo_failure(Result::InvalidArgument);
            }

            Graph working;
            const Result copy_result = make_working_copy(graph, &working);
            if (copy_result != Result::Ok) {
                return undo_failure(copy_result);
            }

            GraphUndoResult result;
            result.result = Result::Ok;

            // Batches are undone in reverse record order so dependent objects are
            // removed before their producers/owners are removed or restored. This
            // keeps link/port/node dependencies valid throughout the working copy.
            for (std::vector<GraphCommandRecord>::const_reverse_iterator it = batch.records.rbegin();
                 it != batch.records.rend();
                 ++it) {
                const Result undo_result = undo_record_in_place(working, *it);
                if (undo_result != Result::Ok) {
                    return undo_failure(undo_result);
                }
                result.applied_records.push_back(*it);
            }

            graph = working;
            return result;
        } catch (const std::bad_alloc&) {
            return undo_failure(Result::AllocationFailure);
        }
    }
}
