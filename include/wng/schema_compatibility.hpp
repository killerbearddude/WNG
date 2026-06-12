// Analyzes whether a graph remains valid when moving between two schemas.
// This layer is read-only: it reports compatibility and affected graph objects
// but does not migrate, repair, or mutate graph or schema state.

#pragma once

#include <vector>

#include <wng/graph_validation.hpp>
#include <wng/ids.hpp>
#include <wng/result.hpp>
#include <wng/schema_diff.hpp>

namespace wng
{
    class Graph;
    class GraphSchema;

    // Summarizes graph validity across the source and target schemas. The status
    // describes validation outcome; analysis failures are reported separately in
    // SchemaCompatibilityReport::result.
    enum class SchemaCompatibilityStatus {
        Compatible,
        SourceInvalid,
        TargetInvalid,
        SourceAndTargetInvalid
    };

    // Diagnostic report for a schema compatibility check. The schema diff and
    // validation reports are retained so future migration tooling can explain
    // why a graph does or does not survive a schema change.
    struct SchemaCompatibilityReport {
        Result result = Result::Ok;
        SchemaCompatibilityStatus status = SchemaCompatibilityStatus::Compatible;

        SchemaDiff schema_diff;

        ValidationReport source_validation;
        ValidationReport target_validation;

        std::vector<NodeId> affected_nodes;
        std::vector<PortId> affected_ports;

        bool compatible() const;
        bool success() const;
    };

    // Compares graph validity under source and target schemas without mutating
    // the graph or either schema. The result is Ok when analysis completed, even
    // if status reports that the graph is invalid under one or both schemas.
    SchemaCompatibilityReport analyze_schema_compatibility(
        const Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema);
}
