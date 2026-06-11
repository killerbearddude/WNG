// Implements in-memory schema snapshot capture and restore for WNG.
// Schema snapshots are value copies for diagnostics and editor draft workflows;
// they are not save files, JSON documents, migrations, or persistence policy.

#include <new>
#include <vector>

#include <wng/schema_snapshot.hpp>

namespace
{
    wng::SchemaSnapshotResult schema_snapshot_failure(wng::Result result)
    {
        wng::SchemaSnapshotResult snapshot_result;
        snapshot_result.result = result;
        return snapshot_result;
    }

    wng::Result restore_definitions_into_schema(
        wng::GraphSchema& schema,
        const std::vector<wng::NodeDefinition>& definitions)
    {
        // Restore uses the public GraphSchema insertion API rather than bypassing
        // schema rules. Invalid or duplicate snapshot definitions are rejected the
        // same way ordinary schema construction would reject them.
        for (const wng::NodeDefinition& definition : definitions) {
            const wng::Result result = schema.add_node_definition(definition);
            if (result != wng::Result::Ok) {
                return result;
            }
        }

        return wng::Result::Ok;
    }
}

namespace wng
{
    bool SchemaSnapshot::empty() const
    {
        return node_definitions.empty();
    }

    bool SchemaSnapshotResult::success() const
    {
        return result == Result::Ok;
    }

    SchemaSnapshotResult capture_schema_snapshot(
        const GraphSchema& schema)
    {
        try {
            SchemaSnapshotResult result;
            result.snapshot.node_definitions = schema.node_definitions();
            result.result = Result::Ok;
            return result;
        } catch (const std::bad_alloc&) {
            return schema_snapshot_failure(Result::AllocationFailure);
        }
    }

    Result restore_schema_snapshot(
        GraphSchema& schema,
        const SchemaSnapshot& snapshot)
    {
        try {
            // Restore is replacement, not merge. The temporary schema protects
            // caller state until every snapshot definition has passed validation.
            GraphSchema replacement;
            const Result result =
                restore_definitions_into_schema(replacement, snapshot.node_definitions);
            if (result != Result::Ok) {
                return result;
            }

            schema = replacement;
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }
}
