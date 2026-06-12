// Implements structural validation for schema migration policy values.
// Policy validation rejects ambiguous mappings before any future migration layer
// can attempt to apply them; no graph or schema mutation occurs here.

#include <wng/schema_migration_policy.hpp>

#include <new>
#include <vector>

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

    const wng::NodeDefinition* find_node_definition(
        const wng::GraphSchema& schema,
        const std::string& node_type)
    {
        return schema.find_node_definition(node_type);
    }

    const wng::PortDefinition* find_port_definition(
        const wng::GraphSchema& schema,
        const wng::PortDefinitionIdentity& identity)
    {
        const wng::NodeDefinition* node = find_node_definition(schema, identity.node_type);
        if (node == nullptr) {
            return nullptr;
        }

        const std::vector<wng::PortDefinition>* definitions = nullptr;
        if (identity.kind == wng::PortKind::Input) {
            definitions = &node->inputs;
        } else if (identity.kind == wng::PortKind::Output) {
            definitions = &node->outputs;
        }

        if (definitions == nullptr) {
            return nullptr;
        }

        for (const wng::PortDefinition& definition : *definitions) {
            if (definition.name == identity.name && definition.kind == identity.kind) {
                return &definition;
            }
        }

        return nullptr;
    }

    bool port_definition_is_required(const wng::PortDefinition& definition)
    {
        return definition.required;
    }

    const std::string& port_definition_type(const wng::PortDefinition& definition)
    {
        return definition.type;
    }

    wng::SchemaMigrationPolicySchemaIssue make_schema_issue(
        wng::SchemaMigrationPolicySchemaIssueKind kind,
        wng::Result result,
        const std::string& node_type)
    {
        wng::SchemaMigrationPolicySchemaIssue issue;
        issue.kind = kind;
        issue.result = result;
        issue.node_type = node_type;
        return issue;
    }

    wng::SchemaMigrationPolicySchemaIssue make_schema_issue(
        wng::SchemaMigrationPolicySchemaIssueKind kind,
        wng::Result result,
        const wng::PortDefinitionIdentity& identity)
    {
        wng::SchemaMigrationPolicySchemaIssue issue;
        issue.kind = kind;
        issue.result = result;
        issue.node_type = identity.node_type;
        issue.port_kind = identity.kind;
        issue.port_name = identity.name;
        return issue;
    }

    void append_node_type_rename_issues(
        const wng::SchemaMigrationPolicy& policy,
        const wng::GraphSchema& source_schema,
        const wng::GraphSchema& target_schema,
        std::vector<wng::SchemaMigrationPolicySchemaIssue>& issues)
    {
        for (const wng::NodeTypeRenamePolicy& rename : policy.node_type_renames) {
            const bool source_from_exists =
                find_node_definition(source_schema, rename.from) != nullptr;
            const bool target_to_exists = find_node_definition(target_schema, rename.to) != nullptr;

            if (!source_from_exists) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::SourceNodeTypeMissing,
                    wng::Result::NotFound,
                    rename.from));
            }

            if (!target_to_exists) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::TargetNodeTypeMissing,
                    wng::Result::NotFound,
                    rename.to));
            }

            if (source_from_exists && target_to_exists &&
                find_node_definition(source_schema, rename.to) != nullptr &&
                find_node_definition(target_schema, rename.from) != nullptr) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::NodeTypeRenameNotNeeded,
                    wng::Result::InvalidArgument,
                    rename.from));
            }
        }
    }

    void append_port_rename_issues(
        const wng::SchemaMigrationPolicy& policy,
        const wng::GraphSchema& source_schema,
        const wng::GraphSchema& target_schema,
        std::vector<wng::SchemaMigrationPolicySchemaIssue>& issues)
    {
        for (const wng::PortDefinitionRenamePolicy& rename : policy.port_renames) {
            const bool source_from_exists =
                find_port_definition(source_schema, rename.from) != nullptr;
            const bool target_to_exists = find_port_definition(target_schema, rename.to) != nullptr;

            if (!source_from_exists) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::SourcePortDefinitionMissing,
                    wng::Result::NotFound,
                    rename.from));
            }

            if (!target_to_exists) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::TargetPortDefinitionMissing,
                    wng::Result::NotFound,
                    rename.to));
            }

            if (source_from_exists && target_to_exists &&
                find_port_definition(source_schema, rename.to) != nullptr &&
                find_port_definition(target_schema, rename.from) != nullptr) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::PortDefinitionRenameNotNeeded,
                    wng::Result::InvalidArgument,
                    rename.from));
            }
        }
    }

    void append_port_type_change_issues(
        const wng::SchemaMigrationPolicy& policy,
        const wng::GraphSchema& source_schema,
        const wng::GraphSchema& target_schema,
        std::vector<wng::SchemaMigrationPolicySchemaIssue>& issues)
    {
        for (const wng::PortTypeChangePolicy& change : policy.port_type_changes) {
            const wng::PortDefinition* source_port = find_port_definition(source_schema, change.port);
            const wng::PortDefinition* target_port = find_port_definition(target_schema, change.port);

            if (source_port == nullptr) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::SourcePortDefinitionMissing,
                    wng::Result::NotFound,
                    change.port));
            }

            if (target_port == nullptr) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::TargetPortDefinitionMissing,
                    wng::Result::NotFound,
                    change.port));
            }

            if (source_port != nullptr && port_definition_type(*source_port) != change.from_type) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::PortTypeChangeSourceMismatch,
                    wng::Result::InvalidArgument,
                    change.port));
            }

            if (target_port != nullptr && port_definition_type(*target_port) != change.to_type) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::PortTypeChangeTargetMismatch,
                    wng::Result::InvalidArgument,
                    change.port));
            }

            if (source_port != nullptr && target_port != nullptr &&
                port_definition_type(*source_port) == port_definition_type(*target_port)) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::PortTypeChangeNotNeeded,
                    wng::Result::InvalidArgument,
                    change.port));
            }
        }
    }

    void append_required_port_default_issues(
        const wng::SchemaMigrationPolicy& policy,
        const wng::GraphSchema& target_schema,
        std::vector<wng::SchemaMigrationPolicySchemaIssue>& issues)
    {
        for (const wng::RequiredPortDefaultPolicy& default_policy : policy.required_port_defaults) {
            const wng::PortDefinition* target_port =
                find_port_definition(target_schema, default_policy.port);
            if (target_port == nullptr) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::RequiredPortDefaultTargetMissing,
                    wng::Result::NotFound,
                    default_policy.port));
                continue;
            }

            if (!port_definition_is_required(*target_port)) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::RequiredPortDefaultNotRequired,
                    wng::Result::InvalidArgument,
                    default_policy.port));
            }
        }
    }

    void append_node_removal_ack_issues(
        const wng::SchemaMigrationPolicy& policy,
        const wng::GraphSchema& source_schema,
        const wng::GraphSchema& target_schema,
        std::vector<wng::SchemaMigrationPolicySchemaIssue>& issues)
    {
        for (const wng::NodeTypeRemovalPolicy& removal : policy.acknowledged_node_removals) {
            if (find_node_definition(source_schema, removal.type) == nullptr) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::SourceNodeTypeMissing,
                    wng::Result::NotFound,
                    removal.type));
            }

            if (find_node_definition(target_schema, removal.type) != nullptr) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::NodeTypeRemovalNotReflectedInTarget,
                    wng::Result::InvalidArgument,
                    removal.type));
            }
        }
    }

    void append_port_removal_ack_issues(
        const wng::SchemaMigrationPolicy& policy,
        const wng::GraphSchema& source_schema,
        const wng::GraphSchema& target_schema,
        std::vector<wng::SchemaMigrationPolicySchemaIssue>& issues)
    {
        for (const wng::PortDefinitionRemovalPolicy& removal : policy.acknowledged_port_removals) {
            if (find_port_definition(source_schema, removal.port) == nullptr) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::SourcePortDefinitionMissing,
                    wng::Result::NotFound,
                    removal.port));
            }

            if (find_port_definition(target_schema, removal.port) != nullptr) {
                issues.push_back(make_schema_issue(
                    wng::SchemaMigrationPolicySchemaIssueKind::PortDefinitionRemovalNotReflectedInTarget,
                    wng::Result::InvalidArgument,
                    removal.port));
            }
        }
    }

    wng::Result first_issue_result(
        const std::vector<wng::SchemaMigrationPolicySchemaIssue>& issues)
    {
        if (issues.empty()) {
            return wng::Result::Ok;
        }

        return issues.front().result;
    }

    wng::SchemaMigrationPolicySchemaValidation policy_schema_validation_failure(
        wng::Result result)
    {
        wng::SchemaMigrationPolicySchemaValidation validation;
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

    bool SchemaMigrationPolicySchemaValidation::success() const
    {
        return result == Result::Ok;
    }

    bool SchemaMigrationPolicySchemaValidation::valid() const
    {
        return result == Result::Ok && issues.empty();
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

    SchemaMigrationPolicySchemaValidation validate_schema_migration_policy(
        const SchemaMigrationPolicy& policy,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema)
    {
        try {
            // Structural validation runs first so schema-aware checks never need
            // to reason about malformed identities, duplicate mappings, or
            // self-renames. The schema-aware layer remains read-only.
            const SchemaMigrationPolicyValidation structural_validation =
                validate_schema_migration_policy(policy);
            if (!structural_validation.success()) {
                SchemaMigrationPolicySchemaValidation validation;
                validation.result = structural_validation.result;

                SchemaMigrationPolicySchemaIssue issue;
                issue.kind = SchemaMigrationPolicySchemaIssueKind::StructuralPolicyInvalid;
                issue.result = structural_validation.result;
                validation.issues.push_back(issue);
                return validation;
            }

            SchemaMigrationPolicySchemaValidation validation;

            // Issues are collected in policy category order and then policy vector
            // order. `result` mirrors the first issue while the full issue list is
            // preserved for deterministic diagnostics.
            append_node_type_rename_issues(
                policy,
                source_schema,
                target_schema,
                validation.issues);
            append_port_rename_issues(policy, source_schema, target_schema, validation.issues);
            append_port_type_change_issues(
                policy,
                source_schema,
                target_schema,
                validation.issues);
            append_required_port_default_issues(policy, target_schema, validation.issues);
            append_node_removal_ack_issues(
                policy,
                source_schema,
                target_schema,
                validation.issues);
            append_port_removal_ack_issues(
                policy,
                source_schema,
                target_schema,
                validation.issues);

            validation.result = first_issue_result(validation.issues);
            return validation;
        } catch (const std::bad_alloc&) {
            return policy_schema_validation_failure(Result::AllocationFailure);
        }
    }
}
