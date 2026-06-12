// Exercises structural validation for schema migration policy values. The tests
// intentionally avoid graph/schema mutation because policy application is a
// future layer.

#include <cassert>
#include <string>

#include <wng/schema_migration_policy.hpp>

namespace
{
    wng::PortDefinitionIdentity port_identity(
        const std::string& node_type = "math.add",
        wng::PortKind kind = wng::PortKind::Input,
        const std::string& name = "value")
    {
        wng::PortDefinitionIdentity identity;
        identity.node_type = node_type;
        identity.kind = kind;
        identity.name = name;
        return identity;
    }

    wng::SchemaMigrationPolicyValidation validate(
        const wng::SchemaMigrationPolicy& policy)
    {
        return wng::validate_schema_migration_policy(policy);
    }
}

int main()
{
    {
        // Empty policy is the no-op baseline. It should be accepted so callers can
        // pass an explicit policy object even when no migration decisions exist.
        const wng::SchemaMigrationPolicy policy;
        const wng::SchemaMigrationPolicyValidation validation = validate(policy);

        assert(policy.empty());
        assert(validation.result == wng::Result::Ok);
        assert(validation.success());
    }

    {
        // Valid node type renames establish the source-to-target mapping shape a
        // future migration application layer may consume.
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.math.add", "math.add" });

        assert(!policy.empty());
        assert(validate(policy).result == wng::Result::Ok);
    }

    {
        // Empty source type is rejected because migration application cannot know
        // which old stable node identity should be transformed.
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "", "math.add" });

        assert(validate(policy).result == wng::Result::InvalidArgument);
    }

    {
        // Empty target type is rejected because it would produce an invalid stable
        // node identity for future migrated graph nodes.
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.math.add", "" });

        assert(validate(policy).result == wng::Result::InvalidArgument);
    }

    {
        // Self-renames are rejected as no-op ambiguity. Callers should omit them
        // rather than making future migration logic special-case no-op policies.
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "math.add", "math.add" });

        assert(validate(policy).result == wng::Result::InvalidArgument);
    }

    {
        // Duplicate source node-type renames are rejected so a future migration
        // application layer never chooses between two mappings for the same type.
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.math.add", "math.add" });
        policy.node_type_renames.push_back({ "old.math.add", "math.sum" });

        assert(validate(policy).result == wng::Result::AlreadyExists);
    }

    {
        // Duplicate target node types are rejected because two old definitions
        // converging on one new type requires explicit merge semantics later.
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.math.add", "math.add" });
        policy.node_type_renames.push_back({ "old.math.sum", "math.add" });

        assert(validate(policy).result == wng::Result::AlreadyExists);
    }

    {
        // Valid port renames preserve the schema-level port identity components
        // needed by future graph migration code.
        wng::SchemaMigrationPolicy policy;
        wng::PortDefinitionRenamePolicy rename;
        rename.from = port_identity("math.add", wng::PortKind::Input, "lhs");
        rename.to = port_identity("math.add", wng::PortKind::Input, "a");
        policy.port_renames.push_back(rename);

        assert(validate(policy).result == wng::Result::Ok);
    }

    {
        // Invalid source port identities are rejected before a policy can encode
        // an unresolvable rename operation.
        wng::SchemaMigrationPolicy policy;
        wng::PortDefinitionRenamePolicy rename;
        rename.from = port_identity("", wng::PortKind::Input, "lhs");
        rename.to = port_identity("math.add", wng::PortKind::Input, "a");
        policy.port_renames.push_back(rename);

        assert(validate(policy).result == wng::Result::InvalidArgument);
    }

    {
        // Self-renaming a port identity is rejected for the same reason as node
        // self-renames: it adds ambiguity without describing a real migration.
        wng::SchemaMigrationPolicy policy;
        wng::PortDefinitionRenamePolicy rename;
        rename.from = port_identity("math.add", wng::PortKind::Input, "value");
        rename.to = port_identity("math.add", wng::PortKind::Input, "value");
        policy.port_renames.push_back(rename);

        assert(validate(policy).result == wng::Result::InvalidArgument);
    }

    {
        // Duplicate source port renames are rejected so a future application layer
        // never has multiple rename destinations for the same old port.
        wng::SchemaMigrationPolicy policy;
        wng::PortDefinitionRenamePolicy first;
        first.from = port_identity("math.add", wng::PortKind::Input, "lhs");
        first.to = port_identity("math.add", wng::PortKind::Input, "a");
        wng::PortDefinitionRenamePolicy second;
        second.from = port_identity("math.add", wng::PortKind::Input, "lhs");
        second.to = port_identity("math.add", wng::PortKind::Input, "b");
        policy.port_renames.push_back(first);
        policy.port_renames.push_back(second);

        assert(validate(policy).result == wng::Result::AlreadyExists);
    }

    {
        // A port type change policy is valid when it names a stable port identity
        // and two distinct type strings.
        wng::SchemaMigrationPolicy policy;
        wng::PortTypeChangePolicy change;
        change.port = port_identity("math.add", wng::PortKind::Input, "value");
        change.from_type = "number";
        change.to_type = "scalar";
        policy.port_type_changes.push_back(change);

        assert(validate(policy).result == wng::Result::Ok);
    }

    {
        // Missing source type information is rejected because future migration
        // code cannot verify the intended compatibility override.
        wng::SchemaMigrationPolicy policy;
        wng::PortTypeChangePolicy change;
        change.port = port_identity();
        change.from_type = "";
        change.to_type = "scalar";
        policy.port_type_changes.push_back(change);

        assert(validate(policy).result == wng::Result::InvalidArgument);
    }

    {
        // Missing target type information is rejected for the same reason: the
        // policy must describe both ends of the compatibility override.
        wng::SchemaMigrationPolicy policy;
        wng::PortTypeChangePolicy change;
        change.port = port_identity();
        change.from_type = "number";
        change.to_type = "";
        policy.port_type_changes.push_back(change);

        assert(validate(policy).result == wng::Result::InvalidArgument);
    }

    {
        // No-op type changes are rejected. Callers should not encode policies
        // that do not actually widen compatibility.
        wng::SchemaMigrationPolicy policy;
        wng::PortTypeChangePolicy change;
        change.port = port_identity();
        change.from_type = "number";
        change.to_type = "number";
        policy.port_type_changes.push_back(change);

        assert(validate(policy).result == wng::Result::InvalidArgument);
    }

    {
        // Required-port defaults are valid policy hints. Empty default strings are
        // allowed because WNG has no runtime value system that can constrain them.
        wng::SchemaMigrationPolicy policy;
        wng::RequiredPortDefaultPolicy default_policy;
        default_policy.port = port_identity("math.add", wng::PortKind::Input, "rhs");
        default_policy.default_value = "";
        policy.required_port_defaults.push_back(default_policy);

        assert(validate(policy).result == wng::Result::Ok);
    }

    {
        // Duplicate required-port defaults are rejected so migration application
        // will have at most one default hint for a missing required port.
        wng::SchemaMigrationPolicy policy;
        wng::RequiredPortDefaultPolicy first;
        first.port = port_identity("math.add", wng::PortKind::Input, "rhs");
        wng::RequiredPortDefaultPolicy second = first;
        second.default_value = "1";
        policy.required_port_defaults.push_back(first);
        policy.required_port_defaults.push_back(second);

        assert(validate(policy).result == wng::Result::AlreadyExists);
    }

    {
        // Acknowledging node removal is valid policy data. It records intent but
        // deliberately does not delete or repair any graph object.
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_node_removals.push_back({ "debug.print" });

        assert(validate(policy).result == wng::Result::Ok);
    }

    {
        // Duplicate node removal acknowledgements are rejected to keep policy data
        // compact and deterministic.
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_node_removals.push_back({ "debug.print" });
        policy.acknowledged_node_removals.push_back({ "debug.print" });

        assert(validate(policy).result == wng::Result::AlreadyExists);
    }

    {
        // Acknowledging port removal is valid when the removed schema port has a
        // complete stable identity.
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_port_removals.push_back({ port_identity() });

        assert(validate(policy).result == wng::Result::Ok);
    }

    {
        // Invalid port-removal identities are rejected before a future migration
        // layer could accidentally acknowledge the wrong removal.
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_port_removals.push_back(
            { port_identity("math.add", wng::PortKind::Input, "") });

        assert(validate(policy).result == wng::Result::InvalidArgument);
    }

    {
        // Duplicate port removal acknowledgements are rejected for deterministic
        // policy interpretation.
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_port_removals.push_back({ port_identity() });
        policy.acknowledged_port_removals.push_back({ port_identity() });

        assert(validate(policy).result == wng::Result::AlreadyExists);
    }

    {
        // Port kind is part of the stable port identity. Invalid enum values are
        // rejected even when all strings are non-empty.
        wng::SchemaMigrationPolicy policy;
        wng::RequiredPortDefaultPolicy default_policy;
        default_policy.port = port_identity(
            "math.add",
            static_cast<wng::PortKind>(99),
            "value");
        policy.required_port_defaults.push_back(default_policy);

        assert(validate(policy).result == wng::Result::InvalidArgument);
    }

    {
        // First failure is deterministic by category and policy order. Node type
        // renames are validated before later invalid port-removal acknowledgements.
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "", "math.add" });
        policy.acknowledged_port_removals.push_back(
            { port_identity("math.add", static_cast<wng::PortKind>(99), "value") });

        assert(validate(policy).result == wng::Result::InvalidArgument);
    }

    {
        // Validation must be read-only: policy authoring tools can validate the
        // same value repeatedly without normalization or hidden mutation.
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.math.add", "math.add" });
        policy.port_type_changes.push_back(
            { port_identity(), "number", "scalar" });
        const std::size_t rename_count = policy.node_type_renames.size();
        const std::size_t type_change_count = policy.port_type_changes.size();
        const std::string original_from = policy.node_type_renames[0].from;
        const std::string original_to_type = policy.port_type_changes[0].to_type;

        assert(validate(policy).result == wng::Result::Ok);
        assert(policy.node_type_renames.size() == rename_count);
        assert(policy.port_type_changes.size() == type_change_count);
        assert(policy.node_type_renames[0].from == original_from);
        assert(policy.port_type_changes[0].to_type == original_to_type);
    }

    return 0;
}
