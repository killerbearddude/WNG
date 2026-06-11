// Captures and restores GraphSchema state as an in-memory value.
// This layer is for schema diagnostics, tests, and future editor drafts; it does
// not define file persistence, JSON, migrations, or schema patching.

#pragma once

#include <cstdint>
#include <vector>

#include <wng/result.hpp>
#include <wng/schema.hpp>

namespace wng
{
    // Names an in-memory schema snapshot. The snapshot owns schema definitions by
    // value and deliberately carries no file, migration, or persistence policy.
    struct SchemaSnapshot {
        // Version for the snapshot wrapper itself. This is not a file format
        // version and must not be treated as persistence metadata.
        std::uint32_t snapshot_version = 1;

        // Complete schema state captured by value. The schema definition model
        // remains the source of schema semantics.
        std::vector<NodeDefinition> node_definitions;

        bool empty() const;
    };

    // Result wrapper for schema snapshot capture. Failed captures leave the
    // snapshot default-constructed rather than exposing a partial copy.
    struct SchemaSnapshotResult {
        Result result = Result::Ok;
        SchemaSnapshot snapshot;

        bool success() const;
    };

    // Captures the current schema into an in-memory snapshot.
    // The schema is copied by value and the source schema is not mutated.
    SchemaSnapshotResult capture_schema_snapshot(
        const GraphSchema& schema);

    // Replaces the target schema with the schema stored in the snapshot.
    // The operation is atomic from the caller's perspective: on failure, the
    // target schema remains unchanged.
    Result restore_schema_snapshot(
        GraphSchema& schema,
        const SchemaSnapshot& snapshot);
}
