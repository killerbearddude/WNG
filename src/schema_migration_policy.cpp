// Implements structural validation for schema migration policy values.
// Policy validation rejects ambiguous mappings before any future migration layer
// can attempt to apply them; no graph or schema mutation occurs here.

#include <wng/schema_migration_policy.hpp>

#include <new>

namespace
{
    bool is_valid_port_kind(wng::PortKind kind)
    {
        return kind == wng::PortKind::Input || kind == wng::PortKind::Output;
    }

    bool node_type_empty(const std::string& type)
    {
        return type.empty();
    }

    bool same_node_type(const std::string& lhs, const std::string& rhs)
    {
        return lhs == rhs;
    }

    bool same_port_identity(
        const wng::PortDefinitionIdentity& lhs,
        const wng::PortDefinitionIdentity& rhs)
    {
        return lhs.node_type == rhs.node_type && lhs.kind == rhs.kind && lhs.name == rhs.name;
    }

    wng::Result validate_port_identity(const wng::PortDefinitionIdentity& identity)
    {
        if (identity.node_type.empty() || identity.name.empty() || !is_valid_port_kind(identity.kind)) {
            return wng::Result::InvalidArgument;
        }

        return wng::Result::Ok;
    }

    wng::Result validate_node_type_renames(const wng::SchemaMigrationPolicy& policy)
    {
        // Ambiguous rename maps are rejected structurally. A future migration
        // application layer must never decide between multiple mappings for the
        // same old or new stable node type.
        for (std::size_t index = 0; index < policy.node_type_renames.size(); ++index) {
            const wng::NodeTypeRenamePolicy& rename = policy.node_type_renames[index];
            if (node_type_empty(rename.from) || node_type_empty(rename.to)) {
                return wng::Result::InvalidArgument;
            }

            if (same_node_type(rename.from, rename.to)) {
                return wng::Result::InvalidArgument;
            }

            for (std::size_t prior = 0; prior < index; ++prior) {
                const wng::NodeTypeRenamePolicy& existing = policy.node_type_renames[prior];
                if (same_node_type(existing.from, rename.from) || same_node_type(existing.to, rename.to)) {
                    return wng::Result::AlreadyExists;
                }
            }
        }

        return wng::Result::Ok;
    }

    wng::Result validate_port_renames(const wng::SchemaMigrationPolicy& policy)
    {
        for (std::size_t index = 0; index < policy.port_renames.size(); ++index) {
            const wng::PortDefinitionRenamePolicy& rename = policy.port_renames[index];
            if (validate_port_identity(rename.from) != wng::Result::Ok ||
                validate_port_identity(rename.to) != wng::Result::Ok) {
                return wng::Result::InvalidArgument;
            }

            if (same_port_identity(rename.from, rename.to)) {
                return wng::Result::InvalidArgument;
            }

            for (std::size_t prior = 0; prior < index; ++prior) {
                const wng::PortDefinitionRenamePolicy& existing = policy.port_renames[prior];
                if (same_port_identity(existing.from, rename.from) ||
                    same_port_identity(existing.to, rename.to)) {
                    return wng::Result::AlreadyExists;
                }
            }
        }

        return wng::Result::Ok;
    }

    wng::Result validate_port_type_changes(const wng::SchemaMigrationPolicy& policy)
    {
        for (std::size_t index = 0; index < policy.port_type_changes.size(); ++index) {
            const wng::PortTypeChangePolicy& change = policy.port_type_changes[index];
            if (validate_port_identity(change.port) != wng::Result::Ok ||
                change.from_type.empty() || change.to_type.empty()) {
                return wng::Result::InvalidArgument;
            }

            if (change.from_type == change.to_type) {
                return wng::Result::InvalidArgument;
            }

            for (std::size_t prior = 0; prior < index; ++prior) {
                if (same_port_identity(policy.port_type_changes[prior].port, change.port)) {
                    return wng::Result::AlreadyExists;
                }
            }
        }

        return wng::Result::Ok;
    }

    wng::Result validate_required_port_defaults(const wng::SchemaMigrationPolicy& policy)
    {
        for (std::size_t index = 0; index < policy.required_port_defaults.size(); ++index) {
            const wng::RequiredPortDefaultPolicy& default_policy =
                policy.required_port_defaults[index];
            if (validate_port_identity(default_policy.port) != wng::Result::Ok) {
                return wng::Result::InvalidArgument;
            }

            for (std::size_t prior = 0; prior < index; ++prior) {
                if (same_port_identity(policy.required_port_defaults[prior].port, default_policy.port)) {
                    return wng::Result::AlreadyExists;
                }
            }
        }

        return wng::Result::Ok;
    }

    wng::Result validate_acknowledged_node_removals(const wng::SchemaMigrationPolicy& policy)
    {
        for (std::size_t index = 0; index < policy.acknowledged_node_removals.size(); ++index) {
            const wng::NodeTypeRemovalPolicy& removal = policy.acknowledged_node_removals[index];
            if (node_type_empty(removal.type)) {
                return wng::Result::InvalidArgument;
            }

            for (std::size_t prior = 0; prior < index; ++prior) {
                if (same_node_type(policy.acknowledged_node_removals[prior].type, removal.type)) {
                    return wng::Result::AlreadyExists;
                }
            }
        }

        return wng::Result::Ok;
    }

    wng::Result validate_acknowledged_port_removals(const wng::SchemaMigrationPolicy& policy)
    {
        for (std::size_t index = 0; index < policy.acknowledged_port_removals.size(); ++index) {
            const wng::PortDefinitionRemovalPolicy& removal =
                policy.acknowledged_port_removals[index];
            if (validate_port_identity(removal.port) != wng::Result::Ok) {
                return wng::Result::InvalidArgument;
            }

            for (std::size_t prior = 0; prior < index; ++prior) {
                if (same_port_identity(policy.acknowledged_port_removals[prior].port, removal.port)) {
                    return wng::Result::AlreadyExists;
                }
            }
        }

        return wng::Result::Ok;
    }

    wng::SchemaMigrationPolicyValidation policy_validation_failure(wng::Result result)
    {
        wng::SchemaMigrationPolicyValidation validation;
        validation.result = result;
        return validation;
    }
}

namespace wng
{
    bool SchemaMigrationPolicy::empty() const
    {
        return node_type_renames.empty() &&
            port_renames.empty() &&
            port_type_changes.empty() &&
            required_port_defaults.empty() &&
            acknowledged_node_removals.empty() &&
            acknowledged_port_removals.empty();
    }

    bool SchemaMigrationPolicyValidation::success() const
    {
        return result == Result::Ok;
    }

    SchemaMigrationPolicyValidation validate_schema_migration_policy(
        const SchemaMigrationPolicy& policy)
    {
        try {
            // Category order defines first-failure determinism. Validation is
            // structural only: it does not inspect specific source/target schemas
            // and does not apply, repair, or mutate graph data.
            const Result node_rename_result = validate_node_type_renames(policy);
            if (node_rename_result != Result::Ok) {
                return policy_validation_failure(node_rename_result);
            }

            const Result port_rename_result = validate_port_renames(policy);
            if (port_rename_result != Result::Ok) {
                return policy_validation_failure(port_rename_result);
            }

            const Result port_type_result = validate_port_type_changes(policy);
            if (port_type_result != Result::Ok) {
                return policy_validation_failure(port_type_result);
            }

            const Result required_default_result = validate_required_port_defaults(policy);
            if (required_default_result != Result::Ok) {
                return policy_validation_failure(required_default_result);
            }

            const Result node_removal_result = validate_acknowledged_node_removals(policy);
            if (node_removal_result != Result::Ok) {
                return policy_validation_failure(node_removal_result);
            }

            const Result port_removal_result = validate_acknowledged_port_removals(policy);
            if (port_removal_result != Result::Ok) {
                return policy_validation_failure(port_removal_result);
            }

            return policy_validation_failure(Result::Ok);
        } catch (const std::bad_alloc&) {
            return policy_validation_failure(Result::AllocationFailure);
        }
    }
}
