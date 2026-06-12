// Defines explicit schema migration policy decisions without applying them.
// Policies are value objects used by future migration planning/application
// layers; they do not mutate graphs, schemas, or snapshots.

#pragma once

#include <string>
#include <vector>

#include <wng/result.hpp>
#include <wng/schema.hpp>

namespace wng
{
    // Identifies the supported structural policy categories. These describe
    // explicit migration intent only; none of them performs a graph/schema edit.
    enum class SchemaMigrationPolicyKind {
        RenameNodeType,
        RenamePortDefinition,
        AllowPortTypeChange,
        ProvideRequiredPortDefault,
        AcknowledgeNodeTypeRemoval,
        AcknowledgePortDefinitionRemoval
    };

    // Declares that graph nodes using one stable schema node type may be mapped
    // to another type by a future migration application layer.
    struct NodeTypeRenamePolicy {
        std::string from;
        std::string to;
    };

    // Stable identity for a schema port definition. It mirrors SchemaDiff's port
    // matching rule: owning node type, port kind, and port name.
    struct PortDefinitionIdentity {
        std::string node_type;
        PortKind kind = PortKind::Input;
        std::string name;
    };

    // Declares that a schema port identity may be renamed by future migration
    // application. This is structural intent only, not an immediate edit.
    struct PortDefinitionRenamePolicy {
        PortDefinitionIdentity from;
        PortDefinitionIdentity to;
    };

    // Declares that a specific port type change is intentionally compatible for
    // future migration logic. It does not coerce current graph data.
    struct PortTypeChangePolicy {
        PortDefinitionIdentity port;
        std::string from_type;
        std::string to_type;
    };

    // Provides a default value hint for future creation of a required port. The
    // value is stored as text because WNG has no runtime value system yet.
    struct RequiredPortDefaultPolicy {
        PortDefinitionIdentity port;
        std::string default_value;
    };

    // Explicitly acknowledges that a node type removal is intentional. Future
    // migration code may use this to distinguish accepted removals from mistakes.
    struct NodeTypeRemovalPolicy {
        std::string type;
    };

    // Explicitly acknowledges that a port definition removal is intentional.
    // This is policy data only and does not remove graph ports.
    struct PortDefinitionRemovalPolicy {
        PortDefinitionIdentity port;
    };

    // Complete structural migration policy. Vectors preserve user/tool-provided
    // order so validation failures are deterministic and easy to audit.
    struct SchemaMigrationPolicy {
        std::vector<NodeTypeRenamePolicy> node_type_renames;
        std::vector<PortDefinitionRenamePolicy> port_renames;
        std::vector<PortTypeChangePolicy> port_type_changes;
        std::vector<RequiredPortDefaultPolicy> required_port_defaults;
        std::vector<NodeTypeRemovalPolicy> acknowledged_node_removals;
        std::vector<PortDefinitionRemovalPolicy> acknowledged_port_removals;

        bool empty() const;
    };

    // Structural validation result for migration policy data. This intentionally
    // reports only a single Result value; richer diagnostics can be layered later.
    struct SchemaMigrationPolicyValidation {
        Result result = Result::Ok;

        bool success() const;
    };

    // Validates policy structure for duplicate entries, empty identities, and
    // self-renames. This does not check whether a policy matches a specific
    // source/target schema pair.
    SchemaMigrationPolicyValidation validate_schema_migration_policy(
        const SchemaMigrationPolicy& policy);
}
