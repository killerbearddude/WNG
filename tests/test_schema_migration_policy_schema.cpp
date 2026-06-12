// Exercises schema-aware validation for migration policy values.
// Tests verify that policies reference concrete source/target schemas correctly
// while preserving the no-mutation boundary required before migration apply.

#include <cassert>
#include <string>
#include <vector>

#include <wng/schema_migration_policy.hpp>

namespace
{
    wng::PortDefinition input(
        const std::string& name,
        const std::string& type = "number",
        bool required = true)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Input;
        definition.type = type;
        definition.required = required;
        return definition;
    }

    wng::PortDefinition output(
        const std::string& name,
        const std::string& type = "number")
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Output;
        definition.type = type;
        return definition;
    }

    wng::NodeDefinition make_node_definition(
        const std::string& type,
        const std::vector<wng::PortDefinition>& inputs = { input("value") },
        const std::vector<wng::PortDefinition>& outputs = { output("result") })
    {
        wng::NodeDefinition definition;
        definition.type = type;
        definition.display_name = type;
        definition.inputs = inputs;
        definition.outputs = outputs;
        return definition;
    }

    wng::GraphSchema make_schema(const std::vector<wng::NodeDefinition>& definitions)
    {
        wng::GraphSchema schema;
        for (const wng::NodeDefinition& definition : definitions) {
            assert(schema.add_node_definition(definition) == wng::Result::Ok);
        }
        return schema;
    }

    wng::PortDefinitionIdentity port_identity(
        const std::string& node_type,
        wng::PortKind kind,
        const std::string& name)
    {
        wng::PortDefinitionIdentity identity;
        identity.node_type = node_type;
        identity.kind = kind;
        identity.name = name;
        return identity;
    }

    bool has_issue(
        const wng::SchemaMigrationPolicySchemaValidation& validation,
        wng::SchemaMigrationPolicySchemaIssueKind kind)
    {
        for (const wng::SchemaMigrationPolicySchemaIssue& issue : validation.issues) {
            if (issue.kind == kind) {
                return true;
            }
        }
        return false;
    }
}

int main()
{
    {
        // Empty policies are schema-valid for any schema pair. This preserves the
        // baseline that policy data is optional until a migration needs it.
        const wng::GraphSchema source;
        const wng::GraphSchema target;
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::Ok);
        assert(validation.success());
        assert(validation.valid());
        assert(validation.issues.empty());
    }

    {
        // Structural validation runs before schema checks. A malformed policy
        // returns a single structural issue and does not inspect source/target schemas.
        const wng::GraphSchema source = make_schema({ make_node_definition("math.add") });
        const wng::GraphSchema target = source;
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "math.add", "math.add" });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::InvalidArgument);
        assert(!validation.success());
        assert(!validation.valid());
        assert(validation.issues.size() == 1);
        assert(validation.issues[0].kind ==
            wng::SchemaMigrationPolicySchemaIssueKind::StructuralPolicyInvalid);
    }

    {
        // A node type rename is schema-valid when the old type exists in the
        // source schema and the new type exists in the target schema.
        const wng::GraphSchema source = make_schema({ make_node_definition("old.math.add") });
        const wng::GraphSchema target = make_schema({ make_node_definition("math.add") });
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.math.add", "math.add" });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.valid());
    }

    {
        // Missing source node types are caught before future migration application
        // can try to map graph nodes from a nonexistent schema definition.
        const wng::GraphSchema source;
        const wng::GraphSchema target = make_schema({ make_node_definition("math.add") });
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "missing", "math.add" });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::NotFound);
        assert(has_issue(validation, wng::SchemaMigrationPolicySchemaIssueKind::SourceNodeTypeMissing));
    }

    {
        // Missing target node types make a rename unusable because the migration
        // has no concrete target schema definition to map to.
        const wng::GraphSchema source = make_schema({ make_node_definition("old.math.add") });
        const wng::GraphSchema target;
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.math.add", "missing" });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::NotFound);
        assert(has_issue(validation, wng::SchemaMigrationPolicySchemaIssueKind::TargetNodeTypeMissing));
    }

    {
        // Port renames are valid when the old port identity exists in the source
        // schema and the new port identity exists in the target schema.
        const wng::GraphSchema source = make_schema({
            make_node_definition("math.add", { input("lhs") })
        });
        const wng::GraphSchema target = make_schema({
            make_node_definition("math.add", { input("a") })
        });
        wng::SchemaMigrationPolicy policy;
        wng::PortDefinitionRenamePolicy rename;
        rename.from = port_identity("math.add", wng::PortKind::Input, "lhs");
        rename.to = port_identity("math.add", wng::PortKind::Input, "a");
        policy.port_renames.push_back(rename);

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.valid());
    }

    {
        // Missing source ports are reported against the old identity so callers
        // can point diagnostics at the invalid policy entry.
        const wng::GraphSchema source = make_schema({ make_node_definition("math.add") });
        const wng::GraphSchema target = make_schema({
            make_node_definition("math.add", { input("a") })
        });
        wng::SchemaMigrationPolicy policy;
        wng::PortDefinitionRenamePolicy rename;
        rename.from = port_identity("math.add", wng::PortKind::Input, "missing");
        rename.to = port_identity("math.add", wng::PortKind::Input, "a");
        policy.port_renames.push_back(rename);

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::NotFound);
        assert(has_issue(
            validation,
            wng::SchemaMigrationPolicySchemaIssueKind::SourcePortDefinitionMissing));
    }

    {
        // Missing target ports make a port rename unusable because the new schema
        // identity does not exist.
        const wng::GraphSchema source = make_schema({ make_node_definition("math.add") });
        const wng::GraphSchema target = make_schema({ make_node_definition("math.add") });
        wng::SchemaMigrationPolicy policy;
        wng::PortDefinitionRenamePolicy rename;
        rename.from = port_identity("math.add", wng::PortKind::Input, "value");
        rename.to = port_identity("math.add", wng::PortKind::Input, "missing");
        policy.port_renames.push_back(rename);

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::NotFound);
        assert(has_issue(
            validation,
            wng::SchemaMigrationPolicySchemaIssueKind::TargetPortDefinitionMissing));
    }

    {
        // Type-change policies are valid only when the source and target schemas
        // agree with the policy's declared from/to type names.
        const wng::GraphSchema source = make_schema({ make_node_definition("math.add") });
        const wng::GraphSchema target = make_schema({
            make_node_definition("math.add", { input("value", "scalar") })
        });
        wng::SchemaMigrationPolicy policy;
        policy.port_type_changes.push_back({
            port_identity("math.add", wng::PortKind::Input, "value"),
            "number",
            "scalar"
        });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.valid());
    }

    {
        // Source type mismatches catch stale or incorrect policy metadata before
        // it can be consumed by a future migration application layer.
        const wng::GraphSchema source = make_schema({ make_node_definition("math.add") });
        const wng::GraphSchema target = make_schema({
            make_node_definition("math.add", { input("value", "scalar") })
        });
        wng::SchemaMigrationPolicy policy;
        policy.port_type_changes.push_back({
            port_identity("math.add", wng::PortKind::Input, "value"),
            "integer",
            "scalar"
        });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::InvalidArgument);
        assert(has_issue(
            validation,
            wng::SchemaMigrationPolicySchemaIssueKind::PortTypeChangeSourceMismatch));
    }

    {
        // Target type mismatches catch policies that do not match the concrete
        // target schema they are meant to describe.
        const wng::GraphSchema source = make_schema({ make_node_definition("math.add") });
        const wng::GraphSchema target = make_schema({
            make_node_definition("math.add", { input("value", "scalar") })
        });
        wng::SchemaMigrationPolicy policy;
        policy.port_type_changes.push_back({
            port_identity("math.add", wng::PortKind::Input, "value"),
            "number",
            "string"
        });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::InvalidArgument);
        assert(has_issue(
            validation,
            wng::SchemaMigrationPolicySchemaIssueKind::PortTypeChangeTargetMismatch));
    }

    {
        // Required-port default policies are valid when the target schema has the
        // referenced required port. The default value itself remains opaque text.
        const wng::GraphSchema source;
        const wng::GraphSchema target = make_schema({ make_node_definition("math.add") });
        wng::SchemaMigrationPolicy policy;
        policy.required_port_defaults.push_back({
            port_identity("math.add", wng::PortKind::Input, "value"),
            "0"
        });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.valid());
    }

    {
        // Missing target ports invalidate default policies because there is no
        // concrete target required port to create later.
        const wng::GraphSchema source;
        const wng::GraphSchema target = make_schema({ make_node_definition("math.add") });
        wng::SchemaMigrationPolicy policy;
        policy.required_port_defaults.push_back({
            port_identity("math.add", wng::PortKind::Input, "missing"),
            "0"
        });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::NotFound);
        assert(has_issue(
            validation,
            wng::SchemaMigrationPolicySchemaIssueKind::RequiredPortDefaultTargetMissing));
    }

    {
        // Defaults for optional ports are rejected by schema-aware validation;
        // the policy is specifically for future creation of missing required ports.
        const wng::GraphSchema source;
        const wng::GraphSchema target = make_schema({
            make_node_definition("math.add", { input("optional", "number", false) })
        });
        wng::SchemaMigrationPolicy policy;
        policy.required_port_defaults.push_back({
            port_identity("math.add", wng::PortKind::Input, "optional"),
            "0"
        });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::InvalidArgument);
        assert(has_issue(
            validation,
            wng::SchemaMigrationPolicySchemaIssueKind::RequiredPortDefaultNotRequired));
    }

    {
        // Node removal acknowledgements are valid only when the type existed in
        // the source schema and is absent from the target schema.
        const wng::GraphSchema source = make_schema({ make_node_definition("legacy.node") });
        const wng::GraphSchema target;
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_node_removals.push_back({ "legacy.node" });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.valid());
    }

    {
        // Acknowledging removal of a missing source type is a policy/schema
        // mismatch, not a valid migration decision.
        const wng::GraphSchema source;
        const wng::GraphSchema target;
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_node_removals.push_back({ "legacy.node" });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::NotFound);
        assert(has_issue(validation, wng::SchemaMigrationPolicySchemaIssueKind::SourceNodeTypeMissing));
    }

    {
        // A removal acknowledgement is invalid when the target still exposes the
        // supposedly removed node type.
        const wng::GraphSchema source = make_schema({ make_node_definition("legacy.node") });
        const wng::GraphSchema target = make_schema({ make_node_definition("legacy.node") });
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_node_removals.push_back({ "legacy.node" });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::InvalidArgument);
        assert(has_issue(
            validation,
            wng::SchemaMigrationPolicySchemaIssueKind::NodeTypeRemovalNotReflectedInTarget));
    }

    {
        // Port removal acknowledgements are valid when the source has the port and
        // the target no longer exposes that port identity.
        const wng::GraphSchema source = make_schema({ make_node_definition("math.add") });
        const wng::GraphSchema target = make_schema({
            make_node_definition("math.add", {}, { output("result") })
        });
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_port_removals.push_back({
            port_identity("math.add", wng::PortKind::Input, "value")
        });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.valid());
    }

    {
        // Missing source ports make removal acknowledgements invalid because the
        // source schema has nothing for future migration logic to remove.
        const wng::GraphSchema source = make_schema({ make_node_definition("math.add") });
        const wng::GraphSchema target;
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_port_removals.push_back({
            port_identity("math.add", wng::PortKind::Input, "missing")
        });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::NotFound);
        assert(has_issue(
            validation,
            wng::SchemaMigrationPolicySchemaIssueKind::SourcePortDefinitionMissing));
    }

    {
        // A removal acknowledgement is invalid when the target still exposes the
        // supposedly removed port definition.
        const wng::GraphSchema source = make_schema({ make_node_definition("math.add") });
        const wng::GraphSchema target = make_schema({ make_node_definition("math.add") });
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_port_removals.push_back({
            port_identity("math.add", wng::PortKind::Input, "value")
        });

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == wng::Result::InvalidArgument);
        assert(has_issue(
            validation,
            wng::SchemaMigrationPolicySchemaIssueKind::PortDefinitionRemovalNotReflectedInTarget));
    }

    {
        // Multiple schema-aware issues are collected deterministically in policy
        // category order. The top-level result mirrors the first issue result.
        const wng::GraphSchema source;
        const wng::GraphSchema target;
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "missing.old", "missing.new" });
        wng::PortTypeChangePolicy type_change;
        type_change.port = port_identity("math.add", wng::PortKind::Input, "value");
        type_change.from_type = "number";
        type_change.to_type = "scalar";
        policy.port_type_changes.push_back(type_change);

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.result == validation.issues.front().result);
        assert(validation.issues.size() >= 3);
        assert(validation.issues[0].kind ==
            wng::SchemaMigrationPolicySchemaIssueKind::SourceNodeTypeMissing);
        assert(validation.issues[1].kind ==
            wng::SchemaMigrationPolicySchemaIssueKind::TargetNodeTypeMissing);
    }

    {
        // Schema-aware validation is read-only. Policy and schema object counts
        // remain unchanged after validation.
        const wng::GraphSchema source = make_schema({ make_node_definition("old.math.add") });
        const wng::GraphSchema target = make_schema({ make_node_definition("math.add") });
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.math.add", "math.add" });
        const std::size_t source_count = source.node_definitions().size();
        const std::size_t target_count = target.node_definitions().size();
        const std::size_t rename_count = policy.node_type_renames.size();

        const wng::SchemaMigrationPolicySchemaValidation validation =
            wng::validate_schema_migration_policy(policy, source, target);

        assert(validation.valid());
        assert(source.node_definitions().size() == source_count);
        assert(target.node_definitions().size() == target_count);
        assert(policy.node_type_renames.size() == rename_count);
        assert(policy.node_type_renames[0].from == "old.math.add");
    }

    return 0;
}
