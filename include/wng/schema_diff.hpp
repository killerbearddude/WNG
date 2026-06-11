// Compares WNG schema definitions by stable schema identity.
// This layer is non-mutating and deterministic; it exists for diagnostics,
// regression tests, future schema migration validation, and editor draft checks.

#pragma once

#include <string>
#include <vector>

#include <wng/result.hpp>
#include <wng/schema.hpp>
#include <wng/schema_snapshot.hpp>

namespace wng
{
    // Describes whether a schema object was added, removed, or changed while
    // comparing two schema states.
    enum class SchemaDiffChange {
        Added,
        Removed,
        Modified
    };

    // Records a changed schema port definition. Port identity is the owning node
    // type, the port kind, and the port name, so type/required/enabled changes are
    // reported as modifications rather than remove/add pairs.
    struct PortDefinitionDiff {
        SchemaDiffChange change = SchemaDiffChange::Modified;

        std::string node_type;
        PortKind kind = PortKind::Input;
        std::string name;

        PortDefinition before;
        PortDefinition after;
    };

    // Records a changed schema node definition. Node identity is the stable node
    // type; display-facing fields can change without changing identity.
    struct NodeDefinitionDiff {
        SchemaDiffChange change = SchemaDiffChange::Modified;

        std::string type;

        NodeDefinition before;
        NodeDefinition after;
    };

    // Result of comparing two schemas. Node and port changes are split so port
    // changes do not force every owning node to appear modified.
    struct SchemaDiff {
        Result result = Result::Ok;

        std::vector<NodeDefinitionDiff> nodes;
        std::vector<PortDefinitionDiff> ports;

        bool empty() const;
        bool changed() const;
        bool success() const;
    };

    // Computes a deterministic diff between two schemas.
    // Node definitions are matched by type. Port definitions are matched by
    // owning node type, port kind, and port name.
    SchemaDiff diff_schemas(
        const GraphSchema& before,
        const GraphSchema& after);

    // Computes a deterministic diff between two schema snapshots by restoring
    // each snapshot into temporary GraphSchema values and delegating to diff_schemas.
    SchemaDiff diff_schema_snapshots(
        const SchemaSnapshot& before,
        const SchemaSnapshot& after);
}
