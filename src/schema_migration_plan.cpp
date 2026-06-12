// Implements read-only schema migration planning for WNG graphs.
// The planner consumes schema compatibility reports and schema diffs to produce
// deterministic diagnostics; it never mutates graphs, schemas, or history.

#include <new>
#include <string>
#include <vector>

#include <wng/schema_migration_plan.hpp>

#include <wng/graph.hpp>
#include <wng/graph_validation.hpp>
#include <wng/schema.hpp>

namespace
{
    wng::SchemaMigrationPlan migration_plan_failure(wng::Result result)
    {
        wng::SchemaMigrationPlan plan;
        plan.result = result;
        return plan;
    }

    bool contains_node_id(const std::vector<wng::NodeId>& ids, wng::NodeId id)
    {
        for (wng::NodeId existing : ids) {
            if (existing == id) {
                return true;
            }
        }

        return false;
    }

    bool contains_port_id(const std::vector<wng::PortId>& ids, wng::PortId id)
    {
        for (wng::PortId existing : ids) {
            if (existing == id) {
                return true;
            }
        }

        return false;
    }

    void append_unique_node(std::vector<wng::NodeId>& ids, wng::NodeId id)
    {
        if (!contains_node_id(ids, id)) {
            ids.push_back(id);
        }
    }

    void append_unique_port(std::vector<wng::PortId>& ids, wng::PortId id)
    {
        if (!contains_port_id(ids, id)) {
            ids.push_back(id);
        }
    }

    bool target_validation_invalid(const wng::SchemaCompatibilityReport& report)
    {
        return report.status == wng::SchemaCompatibilityStatus::TargetInvalid ||
            report.status == wng::SchemaCompatibilityStatus::SourceAndTargetInvalid;
    }

    std::vector<wng::NodeId> nodes_of_type(
        const wng::Graph& graph,
        const std::string& type)
    {
        std::vector<wng::NodeId> nodes;
        for (const wng::Node& node : graph.nodes()) {
            if (node.type == type) {
                nodes.push_back(node.id);
            }
        }

        return nodes;
    }

    bool node_id_in_list(const std::vector<wng::NodeId>& nodes, wng::NodeId id)
    {
        return contains_node_id(nodes, id);
    }

    std::vector<wng::PortId> ports_owned_by_nodes(
        const wng::Graph& graph,
        const std::vector<wng::NodeId>& nodes)
    {
        std::vector<wng::PortId> ports;
        for (const wng::Port& port : graph.ports()) {
            if (node_id_in_list(nodes, port.node)) {
                ports.push_back(port.id);
            }
        }

        return ports;
    }

    std::vector<wng::PortId> matching_ports(
        const wng::Graph& graph,
        const std::string& node_type,
        wng::PortKind kind,
        const std::string& name)
    {
        std::vector<wng::PortId> ports;
        for (const wng::Port& port : graph.ports()) {
            const wng::Node* node = graph.find_node(port.node);
            if (node != nullptr &&
                node->type == node_type &&
                port.kind == kind &&
                port.name == name) {
                ports.push_back(port.id);
            }
        }

        return ports;
    }

    std::vector<wng::NodeId> owners_of_ports(
        const wng::Graph& graph,
        const std::vector<wng::PortId>& ports)
    {
        std::vector<wng::NodeId> owners;

        // Owner reporting follows graph node storage order, while the input port
        // set was collected in graph port storage order. This keeps both exposed
        // ID vectors deterministic for future editor diagnostics.
        for (const wng::Node& node : graph.nodes()) {
            for (const wng::Port& port : graph.ports()) {
                if (port.node == node.id && contains_port_id(ports, port.id)) {
                    append_unique_node(owners, node.id);
                    break;
                }
            }
        }

        return owners;
    }

    bool node_has_matching_port(
        const wng::Graph& graph,
        wng::NodeId node,
        wng::PortKind kind,
        const std::string& name)
    {
        for (const wng::Port& port : graph.ports()) {
            if (port.node == node && port.kind == kind && port.name == name) {
                return true;
            }
        }

        return false;
    }

    std::vector<wng::NodeId> nodes_missing_required_port(
        const wng::Graph& graph,
        const std::string& node_type,
        wng::PortKind kind,
        const std::string& name)
    {
        std::vector<wng::NodeId> nodes;
        for (const wng::Node& node : graph.nodes()) {
            if (node.type == node_type && !node_has_matching_port(graph, node.id, kind, name)) {
                nodes.push_back(node.id);
            }
        }

        return nodes;
    }

    bool target_issue_references_any_node(
        const wng::ValidationReport& report,
        const std::vector<wng::NodeId>& nodes)
    {
        for (const wng::ValidationIssue& issue : report.issues) {
            if (contains_node_id(nodes, issue.node)) {
                return true;
            }
        }

        return false;
    }

    bool target_issue_references_any_port(
        const wng::ValidationReport& report,
        const std::vector<wng::PortId>& ports)
    {
        for (const wng::ValidationIssue& issue : report.issues) {
            if (contains_port_id(ports, issue.port)) {
                return true;
            }
        }

        return false;
    }

    bool any_action_blocking(const std::vector<wng::SchemaMigrationAction>& actions)
    {
        for (const wng::SchemaMigrationAction& action : actions) {
            if (action.blocking) {
                return true;
            }
        }

        return false;
    }

    void append_removed_node_actions(
        const wng::Graph& graph,
        const wng::SchemaDiff& diff,
        std::vector<wng::SchemaMigrationAction>& actions)
    {
        for (const wng::NodeDefinitionDiff& node_diff : diff.nodes) {
            if (node_diff.change != wng::SchemaDiffChange::Removed) {
                continue;
            }

            wng::SchemaMigrationAction action;
            action.kind = wng::SchemaMigrationActionKind::RemoveNodeType;
            action.node_type = node_diff.type;
            action.affected_nodes = nodes_of_type(graph, node_diff.type);
            action.affected_ports = ports_owned_by_nodes(graph, action.affected_nodes);
            action.blocking = !action.affected_nodes.empty();
            actions.push_back(action);
        }
    }

    void append_modified_node_actions(
        const wng::Graph& graph,
        const wng::SchemaCompatibilityReport& compatibility,
        std::vector<wng::SchemaMigrationAction>& actions)
    {
        for (const wng::NodeDefinitionDiff& node_diff : compatibility.schema_diff.nodes) {
            if (node_diff.change != wng::SchemaDiffChange::Modified) {
                continue;
            }

            wng::SchemaMigrationAction action;
            action.kind = wng::SchemaMigrationActionKind::ModifyNodeType;
            action.node_type = node_diff.type;
            action.affected_nodes = nodes_of_type(graph, node_diff.type);
            action.blocking = target_validation_invalid(compatibility) &&
                target_issue_references_any_node(
                    compatibility.target_validation,
                    action.affected_nodes);
            actions.push_back(action);
        }
    }

    void append_removed_port_actions(
        const wng::Graph& graph,
        const wng::SchemaDiff& diff,
        std::vector<wng::SchemaMigrationAction>& actions)
    {
        for (const wng::PortDefinitionDiff& port_diff : diff.ports) {
            if (port_diff.change != wng::SchemaDiffChange::Removed) {
                continue;
            }

            wng::SchemaMigrationAction action;
            action.kind = wng::SchemaMigrationActionKind::RemovePortDefinition;
            action.node_type = port_diff.node_type;
            action.port_kind = port_diff.kind;
            action.port_name = port_diff.name;
            action.affected_ports = matching_ports(
                graph,
                port_diff.node_type,
                port_diff.kind,
                port_diff.name);
            action.affected_nodes = owners_of_ports(graph, action.affected_ports);
            action.blocking = !action.affected_ports.empty();
            actions.push_back(action);
        }
    }

    void append_modified_port_actions(
        const wng::Graph& graph,
        const wng::SchemaCompatibilityReport& compatibility,
        std::vector<wng::SchemaMigrationAction>& actions)
    {
        for (const wng::PortDefinitionDiff& port_diff : compatibility.schema_diff.ports) {
            if (port_diff.change != wng::SchemaDiffChange::Modified) {
                continue;
            }

            wng::SchemaMigrationAction action;
            action.kind = wng::SchemaMigrationActionKind::ModifyPortDefinition;
            action.node_type = port_diff.node_type;
            action.port_kind = port_diff.kind;
            action.port_name = port_diff.name;
            action.affected_ports = matching_ports(
                graph,
                port_diff.node_type,
                port_diff.kind,
                port_diff.name);
            action.affected_nodes = owners_of_ports(graph, action.affected_ports);
            action.blocking = target_validation_invalid(compatibility) &&
                target_issue_references_any_port(
                    compatibility.target_validation,
                    action.affected_ports);
            actions.push_back(action);
        }
    }

    void append_added_required_port_actions(
        const wng::Graph& graph,
        const wng::SchemaDiff& diff,
        std::vector<wng::SchemaMigrationAction>& actions)
    {
        for (const wng::PortDefinitionDiff& port_diff : diff.ports) {
            if (port_diff.change != wng::SchemaDiffChange::Added || !port_diff.after.required) {
                continue;
            }

            wng::SchemaMigrationAction action;
            action.kind = wng::SchemaMigrationActionKind::AddRequiredPort;
            action.node_type = port_diff.node_type;
            action.port_kind = port_diff.kind;
            action.port_name = port_diff.name;
            action.affected_nodes = nodes_missing_required_port(
                graph,
                port_diff.node_type,
                port_diff.kind,
                port_diff.name);
            action.blocking = !action.affected_nodes.empty();
            actions.push_back(action);
        }
    }

    void append_target_validation_fallback(
        const wng::SchemaCompatibilityReport& compatibility,
        std::vector<wng::SchemaMigrationAction>& actions)
    {
        if (!target_validation_invalid(compatibility) || any_action_blocking(actions)) {
            return;
        }

        wng::SchemaMigrationAction action;
        action.kind = wng::SchemaMigrationActionKind::TargetValidationIssue;
        action.affected_nodes = compatibility.affected_nodes;
        action.affected_ports = compatibility.affected_ports;
        action.blocking = true;
        actions.push_back(action);
    }

    bool same_port_identity(
        const wng::SchemaMigrationAction& action,
        const wng::PortDefinitionIdentity& identity)
    {
        return action.node_type == identity.node_type &&
            action.port_kind == identity.kind &&
            action.port_name == identity.name;
    }

    bool covered_by_node_rename(
        const wng::SchemaMigrationAction& action,
        const wng::SchemaMigrationPolicy& policy)
    {
        for (const wng::NodeTypeRenamePolicy& rename : policy.node_type_renames) {
            if (rename.from == action.node_type || rename.to == action.node_type) {
                return true;
            }
        }

        return false;
    }

    bool covered_by_node_removal_ack(
        const wng::SchemaMigrationAction& action,
        const wng::SchemaMigrationPolicy& policy)
    {
        for (const wng::NodeTypeRemovalPolicy& removal : policy.acknowledged_node_removals) {
            if (removal.type == action.node_type) {
                return true;
            }
        }

        return false;
    }

    bool covered_by_port_rename(
        const wng::SchemaMigrationAction& action,
        const wng::SchemaMigrationPolicy& policy)
    {
        for (const wng::PortDefinitionRenamePolicy& rename : policy.port_renames) {
            if (same_port_identity(action, rename.from) ||
                same_port_identity(action, rename.to)) {
                return true;
            }
        }

        return false;
    }

    bool covered_by_port_type_change(
        const wng::SchemaMigrationAction& action,
        const wng::SchemaMigrationPolicy& policy)
    {
        for (const wng::PortTypeChangePolicy& type_change : policy.port_type_changes) {
            if (same_port_identity(action, type_change.port)) {
                return true;
            }
        }

        return false;
    }

    bool covered_by_required_port_default(
        const wng::SchemaMigrationAction& action,
        const wng::SchemaMigrationPolicy& policy)
    {
        for (const wng::RequiredPortDefaultPolicy& default_policy :
            policy.required_port_defaults) {
            if (same_port_identity(action, default_policy.port)) {
                return true;
            }
        }

        return false;
    }

    bool covered_by_port_removal_ack(
        const wng::SchemaMigrationAction& action,
        const wng::SchemaMigrationPolicy& policy)
    {
        for (const wng::PortDefinitionRemovalPolicy& removal :
            policy.acknowledged_port_removals) {
            if (same_port_identity(action, removal.port)) {
                return true;
            }
        }

        return false;
    }

    bool action_is_policy_covered(
        const wng::SchemaMigrationAction& action,
        const wng::SchemaMigrationPolicy& policy)
    {
        // Coverage is diagnostic only. It records that explicit policy data
        // matches the action, but it does not mutate graph state or prove that a
        // migration has been applied.
        switch (action.kind) {
        case wng::SchemaMigrationActionKind::RemoveNodeType:
            return covered_by_node_removal_ack(action, policy) ||
                covered_by_node_rename(action, policy);
        case wng::SchemaMigrationActionKind::ModifyNodeType:
            return covered_by_node_rename(action, policy);
        case wng::SchemaMigrationActionKind::RemovePortDefinition:
            return covered_by_port_removal_ack(action, policy) ||
                covered_by_port_rename(action, policy);
        case wng::SchemaMigrationActionKind::ModifyPortDefinition:
            // SchemaMigrationAction stores stable port identity but not before/after
            // port types, so this patch covers type-change policies by identity
            // only. Type-alignment diagnostics belong to a richer future report.
            return covered_by_port_type_change(action, policy) ||
                covered_by_port_rename(action, policy);
        case wng::SchemaMigrationActionKind::AddRequiredPort:
            return covered_by_required_port_default(action, policy);
        case wng::SchemaMigrationActionKind::None:
        case wng::SchemaMigrationActionKind::TargetValidationIssue:
            return false;
        }

        return false;
    }

    void apply_policy_coverage(
        wng::SchemaMigrationPlan& plan,
        const wng::SchemaMigrationPolicy& policy)
    {
        // Policy coverage intentionally leaves blocking unchanged. Future
        // migration application may consume covered actions, but this planning
        // layer remains read-only and conservative.
        for (wng::SchemaMigrationAction& action : plan.actions) {
            action.policy_covered = action_is_policy_covered(action, policy);
        }
    }
}

namespace wng
{
    bool SchemaMigrationPlan::success() const
    {
        return result == Result::Ok;
    }

    bool SchemaMigrationPlan::compatible() const
    {
        return result == Result::Ok && compatibility.compatible();
    }

    bool SchemaMigrationPlan::blocked() const
    {
        return any_action_blocking(actions);
    }

    bool SchemaMigrationPlan::empty() const
    {
        return actions.empty();
    }

    SchemaMigrationPlan build_schema_migration_plan(
        const Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema)
    {
        try {
            SchemaMigrationPlan plan;

            // Planning is a read-only layer over compatibility analysis. It does
            // not execute commands, mutate graph/schema state, or apply repair
            // policies; it only turns schema diffs into deterministic diagnostics.
            plan.compatibility = analyze_schema_compatibility(
                graph,
                source_schema,
                target_schema);
            if (!plan.compatibility.success()) {
                plan.result = plan.compatibility.result;
                return plan;
            }

            // Action order is explicit and stable. Future migration application
            // can rely on this ordering, but it must be implemented separately.
            append_removed_node_actions(
                graph,
                plan.compatibility.schema_diff,
                plan.actions);
            append_modified_node_actions(graph, plan.compatibility, plan.actions);
            append_removed_port_actions(
                graph,
                plan.compatibility.schema_diff,
                plan.actions);
            append_modified_port_actions(graph, plan.compatibility, plan.actions);
            append_added_required_port_actions(
                graph,
                plan.compatibility.schema_diff,
                plan.actions);
            append_target_validation_fallback(plan.compatibility, plan.actions);

            plan.result = Result::Ok;
            return plan;
        } catch (const std::bad_alloc&) {
            return migration_plan_failure(Result::AllocationFailure);
        }
    }

    SchemaMigrationPlan build_schema_migration_plan(
        const Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema,
        const SchemaMigrationPolicy& policy)
    {
        try {
            const SchemaMigrationPolicyValidation policy_validation =
                validate_schema_migration_policy(policy);
            if (!policy_validation.success()) {
                return migration_plan_failure(policy_validation.result);
            }

            SchemaMigrationPlan plan = build_schema_migration_plan(
                graph,
                source_schema,
                target_schema);
            if (!plan.success()) {
                return plan;
            }

            apply_policy_coverage(plan, policy);
            return plan;
        } catch (const std::bad_alloc&) {
            return migration_plan_failure(Result::AllocationFailure);
        }
    }
}
