// Provides the product-facing in-memory working-state boundary for WNG.
// GraphSession groups graph data, schema data, mixed graph history, validation,
// planning, and DTO handoff APIs without introducing editor UI, rendering,
// WPL, persistence, or runtime evaluation ownership.

#pragma once

#include <cstdint>

#include <wng/execution_plan.hpp>
#include <wng/graph.hpp>
#include <wng/graph_command.hpp>
#include <wng/graph_history.hpp>
#include <wng/graph_validation.hpp>
#include <wng/result.hpp>
#include <wng/schema.hpp>
#include <wng/schema_migration_apply_command.hpp>
#include <wng/serialization_dto.hpp>

namespace wng
{
    // Reports both halves of a session-routed command operation. Graph mutation
    // happens before history recording because the existing command helpers own
    // command execution; callers need both results when mutation succeeds but
    // history recording fails due to allocation or record validation.
    struct GraphSessionCommandResult {
        GraphCommandResult command;
        Result history_result = Result::Ok;

        // Returns true only when the graph command succeeded and its successful
        // record was accepted by the session history owner.
        bool success() const;
    };

    // Owns one in-memory graph working state for embedders. This class is the
    // first product-shaped API over the existing graph-core primitives; it does
    // not own editor UI, WPL rendering, persistence, or runtime evaluation state.
    class GraphSession {
    public:
        // Returns the mutable graph owned by this session. Direct mutations are
        // allowed for low-level embedders, but callers must use mark_modified()
        // if they bypass session recording helpers and still want dirty/revision
        // tracking to reflect the change.
        Graph& graph();

        // Returns the read-only graph owned by this session.
        const Graph& graph() const;

        // Returns the mutable schema owned by this session. Direct schema edits do
        // not automatically produce graph history entries; callers should use
        // mark_modified() when changing schema through this accessor.
        GraphSchema& schema();

        // Returns the read-only schema owned by this session.
        const GraphSchema& schema() const;

        // Returns the mixed graph-level history owned by this session. This is
        // exposed so product layers can inspect counts or clear history while the
        // session remains the normal route for undo/redo graph mutation.
        GraphHistory& history();

        // Returns the read-only mixed graph-level history owned by this session.
        const GraphHistory& history() const;

        // Returns the monotonically increasing in-memory revision. Revisions are
        // process-local change counters and are not stable persistent IDs.
        std::uint64_t revision() const;

        // Reports whether the current revision differs from the last saved mark.
        // The saved mark is controlled only by mark_saved().
        bool dirty() const;

        // Marks the current revision as externally saved or accepted. This does
        // not write files, clear history, or mutate graph/schema data.
        void mark_saved();

        // Marks the session as changed after a caller mutates graph, schema, or
        // history through direct mutable accessors. Session helper methods call
        // this internally after graph state changes succeed.
        void mark_modified();

        // Clears the mixed history stacks without changing graph/schema data. This
        // is useful after loading a graph as a fresh user baseline.
        void clear_history();

        // Validates the session graph against the session schema using the default
        // graph/schema validation options.
        ValidationReport validate() const;

        // Validates the session graph against the session schema using explicit
        // graph and schema validation options.
        ValidationReport validate(const GraphSchemaValidationOptions& options) const;

        // Builds deterministic, non-executing execution metadata from the session
        // graph and schema. This does not evaluate nodes or store runtime values.
        ExecutionPlan build_execution_plan(const ExecutionPlanRequest& request) const;

        // Exports the session graph through the format-neutral in-memory DTO
        // boundary. This function performs no file I/O.
        Result export_graph(GraphDto* out_graph) const;

        // Imports graph data through the existing atomic DTO boundary. On success,
        // history is cleared because old undo entries refer to the previous graph.
        Result import_graph(const GraphDto& graph_dto);

        // Executes a graph-core node creation command, records the successful
        // command in session history, and marks the session modified when graph
        // state changes.
        GraphSessionCommandResult create_node(const NodeDesc& desc);

        // Executes a schema-aware node creation command using the session schema.
        // This validates node type availability without instantiating schema ports.
        GraphSessionCommandResult create_schema_node(const NodeDesc& desc);

        // Executes schema-defined node instantiation using the session schema. On
        // success, the command record captures the created node and schema ports.
        GraphSessionCommandResult instantiate_node(const NodeDesc& desc);

        // Executes a graph-core node destruction command, records the successful
        // command in session history, and preserves the command's mutation summary.
        GraphSessionCommandResult destroy_node(NodeId node);

        // Executes a graph-core port creation command against the owned graph and
        // records the successful command in session history.
        GraphSessionCommandResult add_port(NodeId node, const PortDesc& desc);

        // Executes a schema-aware port creation command using the session schema.
        // The schema decides whether the node type allows the requested port.
        GraphSessionCommandResult add_schema_port(NodeId node, const PortDesc& desc);

        // Executes a graph-core port removal command against the owned graph and
        // records removed-port and dependent-link metadata in session history.
        GraphSessionCommandResult remove_port(PortId port);

        // Executes a graph-core link creation command against the owned graph and
        // records the successful command in session history.
        GraphSessionCommandResult create_link(PortId from, PortId to);

        // Executes a schema-aware link creation command using the session schema.
        // Built-in and schema connection rules are both applied before success.
        GraphSessionCommandResult create_schema_link(PortId from, PortId to);

        // Executes a graph-core link destruction command against the owned graph
        // and records the removed-link snapshot in session history.
        GraphSessionCommandResult destroy_link(LinkId link);

        // Records one successful graph command in the mixed history and advances
        // the session revision only if the record is accepted by GraphHistory.
        Result record_graph_command(const GraphCommandRecord& record);

        // Records one successful graph command batch in the mixed history and
        // advances the session revision only if the batch is accepted.
        Result record_graph_command_batch(const GraphCommandBatch& batch);

        // Records one successful schema migration apply command in the mixed
        // history and advances the session revision only if the record is accepted.
        Result record_schema_migration_apply_command(
            const SchemaMigrationApplyCommandRecord& record);

        // Undoes the most recent mixed graph history entry against the session
        // graph and advances the revision only after graph mutation succeeds.
        GraphHistoryOperationResult undo();

        // Redoes the most recent mixed graph history entry against the session
        // graph and advances the revision only after graph mutation succeeds.
        GraphHistoryOperationResult redo();

    private:
        GraphSessionCommandResult record_executed_command(
            const GraphCommandResult& command_result);

        Graph graph_;
        GraphSchema schema_;
        GraphHistory history_;
        std::uint64_t revision_ = 0;
        std::uint64_t saved_revision_ = 0;
    };
}
