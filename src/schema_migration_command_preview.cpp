// Implements read-only command previews for schema migration.
// The preview reports prospective graph-level operations that a future migration
// apply layer may perform, but it never mutates graph or schema state.

#include <wng/schema_migration_command_preview.hpp>

#include <new>

#include <wng/graph.hpp>
#include <wng/schema.hpp>

namespace
{
    wng::SchemaMigrationCommandPreview preview_failure(
        wng::Result result,
        wng::SchemaMigrationCommandPreviewStatus status)
    {
        wng::SchemaMigrationCommandPreview preview;
        preview.result = result;
        preview.status = status;
        return preview;
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

    std::vector<wng::NodeId> nodes_of_type(
        const wng::Graph& graph,
        const std::string& node_type)
    {
        std::vector<wng::NodeId> nodes;
        for (const wng::Node& node : graph.nodes()) {
            if (node.type == node_type) {
                nodes.push_back(node.id);
            }
        }

        return nodes;
    }

    std::vector<wng::PortId> ports_owned_by_nodes(
        const wng::Graph& graph,
        const std::vector<wng::NodeId>& nodes)
    {
        std::vector<wng::PortId> ports;
        for (const wng::Port& port : graph.ports()) {
            if (contains_node_id(nodes, port.node)) {
                ports.push_back(port.id);
            }
        }

        return ports;
    }

    bool port_matches_identity(
        const wng::Graph& graph,
        const wng::Port& port,
        const wng::PortDefinitionIdentity& identity)
    {
        const wng::Node* node = graph.find_node(port.node);
        return node != nullptr &&
            node->type == identity.node_type &&
            port.kind == identity.kind &&
            port.name == identity.name;
    }

    std::vector<wng::PortId> matching_ports(
        const wng::Graph& graph,
        const wng::PortDefinitionIdentity& identity)
    {
        std::vector<wng::PortId> ports;
        for (const wng::Port& port : graph.ports()) {
            if (port_matches_identity(graph, port, identity)) {
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
        const wng::PortDefinitionIdentity& identity)
    {
        for (const wng::Port& port : graph.ports()) {
            if (port.node == node && port.kind == identity.kind && port.name == identity.name) {
                return true;
            }
        }

        return false;
    }

    std::vector<wng::NodeId> nodes_missing_required_port(
        const wng::Graph& graph,
        const wng::PortDefinitionIdentity& identity)
    {
        std::vector<wng::NodeId> nodes;
        for (const wng::Node& node : graph.nodes()) {
            if (node.type == identity.node_type &&
                !node_has_matching_port(graph, node.id, identity)) {
                nodes.push_back(node.id);
            }
        }

        return nodes;
    }

    bool action_matches_port_identity(
        const wng::SchemaMigrationAction& action,
        const wng::PortDefinitionIdentity& identity)
    {
        return action.node_type == identity.node_type &&
            action.port_kind == identity.kind &&
            action.port_name == identity.name;
    }

    bool has_covered_action_for_node_type(
        const wng::SchemaMigrationApplyPreview& preview,
        wng::SchemaMigrationActionKind kind,
        const std::string& node_type)
    {
        for (const wng::SchemaMigrationAction& action : preview.plan.actions) {
            if (action.kind == kind && action.policy_covered && action.node_type == node_type) {
                return true;
            }
        }

        return false;
    }

    bool has_covered_action_for_port(
        const wng::SchemaMigrationApplyPreview& preview,
        wng::SchemaMigrationActionKind kind,
        const wng::PortDefinitionIdentity& identity)
    {
        for (const wng::SchemaMigrationAction& action : preview.plan.actions) {
            if (action.kind == kind && action.policy_covered &&
                action_matches_port_identity(action, identity)) {
                return true;
            }
        }

        return false;
    }

    void append_node_rename_steps(
        const wng::Graph& graph,
        const wng::SchemaMigrationApplyPreview& preview,
        const wng::SchemaMigrationPolicy& policy,
        std::vector<wng::SchemaMigrationCommandPreviewStep>& steps)
    {
        for (const wng::NodeTypeRenamePolicy& rename : policy.node_type_renames) {
            if (!has_covered_action_for_node_type(
                    preview,
                    wng::SchemaMigrationActionKind::RemoveNodeType,
                    rename.from) &&
                !has_covered_action_for_node_type(
                    preview,
                    wng::SchemaMigrationActionKind::ModifyNodeType,
                    rename.from)) {
                continue;
            }

            wng::SchemaMigrationCommandPreviewStep step;
            step.kind = wng::SchemaMigrationCommandPreviewStepKind::RenameNodeType;
            step.from_node_type = rename.from;
            step.to_node_type = rename.to;
            step.affected_nodes = nodes_of_type(graph, rename.from);
            step.destructive = false;
            step.policy_covered = true;
            if (!step.affected_nodes.empty()) {
                steps.push_back(step);
            }
        }
    }

    void append_port_rename_steps(
        const wng::Graph& graph,
        const wng::SchemaMigrationApplyPreview& preview,
        const wng::SchemaMigrationPolicy& policy,
        std::vector<wng::SchemaMigrationCommandPreviewStep>& steps)
    {
        for (const wng::PortDefinitionRenamePolicy& rename : policy.port_renames) {
            if (!has_covered_action_for_port(
                    preview,
                    wng::SchemaMigrationActionKind::RemovePortDefinition,
                    rename.from) &&
                !has_covered_action_for_port(
                    preview,
                    wng::SchemaMigrationActionKind::ModifyPortDefinition,
                    rename.from)) {
                continue;
            }

            wng::SchemaMigrationCommandPreviewStep step;
            step.kind = wng::SchemaMigrationCommandPreviewStepKind::RenamePortDefinition;
            step.from_port = rename.from;
            step.to_port = rename.to;
            step.affected_ports = matching_ports(graph, rename.from);
            step.affected_nodes = owners_of_ports(graph, step.affected_ports);
            step.destructive = false;
            step.policy_covered = true;
            if (!step.affected_ports.empty()) {
                steps.push_back(step);
            }
        }
    }

    void append_port_type_change_steps(
        const wng::Graph& graph,
        const wng::SchemaMigrationApplyPreview& preview,
        const wng::SchemaMigrationPolicy& policy,
        std::vector<wng::SchemaMigrationCommandPreviewStep>& steps)
    {
        for (const wng::PortTypeChangePolicy& type_change : policy.port_type_changes) {
            if (!has_covered_action_for_port(
                    preview,
                    wng::SchemaMigrationActionKind::ModifyPortDefinition,
                    type_change.port)) {
                continue;
            }

            wng::SchemaMigrationCommandPreviewStep step;
            step.kind = wng::SchemaMigrationCommandPreviewStepKind::ChangePortType;
            step.from_port = type_change.port;
            step.to_port = type_change.port;
            step.from_type = type_change.from_type;
            step.to_type = type_change.to_type;
            step.affected_ports = matching_ports(graph, type_change.port);
            step.affected_nodes = owners_of_ports(graph, step.affected_ports);
            step.destructive = false;
            step.policy_covered = true;
            if (!step.affected_ports.empty()) {
                steps.push_back(step);
            }
        }
    }

    void append_required_port_default_steps(
        const wng::Graph& graph,
        const wng::SchemaMigrationApplyPreview& preview,
        const wng::SchemaMigrationPolicy& policy,
        std::vector<wng::SchemaMigrationCommandPreviewStep>& steps)
    {
        for (const wng::RequiredPortDefaultPolicy& default_policy :
            policy.required_port_defaults) {
            if (!has_covered_action_for_port(
                    preview,
                    wng::SchemaMigrationActionKind::AddRequiredPort,
                    default_policy.port)) {
                continue;
            }

            wng::SchemaMigrationCommandPreviewStep step;
            step.kind = wng::SchemaMigrationCommandPreviewStepKind::AddRequiredPort;
            step.to_port = default_policy.port;
            step.default_value = default_policy.default_value;
            step.affected_nodes = nodes_missing_required_port(graph, default_policy.port);
            step.destructive = false;
            step.policy_covered = true;
            if (!step.affected_nodes.empty()) {
                steps.push_back(step);
            }
        }
    }

    void append_node_removal_steps(
        const wng::Graph& graph,
        const wng::SchemaMigrationApplyPreview& preview,
        const wng::SchemaMigrationPolicy& policy,
        std::vector<wng::SchemaMigrationCommandPreviewStep>& steps)
    {
        for (const wng::NodeTypeRemovalPolicy& removal : policy.acknowledged_node_removals) {
            if (!has_covered_action_for_node_type(
                    preview,
                    wng::SchemaMigrationActionKind::RemoveNodeType,
                    removal.type)) {
                continue;
            }

            wng::SchemaMigrationCommandPreviewStep step;
            step.kind = wng::SchemaMigrationCommandPreviewStepKind::RemoveNodesForRemovedType;
            step.from_node_type = removal.type;
            step.affected_nodes = nodes_of_type(graph, removal.type);
            step.affected_ports = ports_owned_by_nodes(graph, step.affected_nodes);
            step.destructive = true;
            step.policy_covered = true;
            if (!step.affected_nodes.empty()) {
                steps.push_back(step);
            }
        }
    }

    void append_port_removal_steps(
        const wng::Graph& graph,
        const wng::SchemaMigrationApplyPreview& preview,
        const wng::SchemaMigrationPolicy& policy,
        std::vector<wng::SchemaMigrationCommandPreviewStep>& steps)
    {
        for (const wng::PortDefinitionRemovalPolicy& removal :
            policy.acknowledged_port_removals) {
            if (!has_covered_action_for_port(
                    preview,
                    wng::SchemaMigrationActionKind::RemovePortDefinition,
                    removal.port)) {
                continue;
            }

            wng::SchemaMigrationCommandPreviewStep step;
            step.kind = wng::SchemaMigrationCommandPreviewStepKind::RemovePortsForRemovedDefinition;
            step.from_port = removal.port;
            step.affected_ports = matching_ports(graph, removal.port);
            step.affected_nodes = owners_of_ports(graph, step.affected_ports);
            step.destructive = true;
            step.policy_covered = true;
            if (!step.affected_ports.empty()) {
                steps.push_back(step);
            }
        }
    }

    void append_preview_steps(
        const wng::Graph& graph,
        const wng::SchemaMigrationApplyPreview& apply_preview,
        const wng::SchemaMigrationPolicy& policy,
        std::vector<wng::SchemaMigrationCommandPreviewStep>& steps)
    {
        // Step order follows policy category order. This keeps command previews
        // deterministic without depending on a future command executor.
        append_node_rename_steps(graph, apply_preview, policy, steps);
        append_port_rename_steps(graph, apply_preview, policy, steps);
        append_port_type_change_steps(graph, apply_preview, policy, steps);
        append_required_port_default_steps(graph, apply_preview, policy, steps);
        append_node_removal_steps(graph, apply_preview, policy, steps);
        append_port_removal_steps(graph, apply_preview, policy, steps);
    }
}

namespace wng
{
    bool SchemaMigrationCommandPreview::success() const
    {
        return result == Result::Ok;
    }

    bool SchemaMigrationCommandPreview::ready() const
    {
        return result == Result::Ok && status == SchemaMigrationCommandPreviewStatus::Ready;
    }

    bool SchemaMigrationCommandPreview::blocked() const
    {
        return !ready();
    }

    bool SchemaMigrationCommandPreview::empty() const
    {
        return steps.empty();
    }

    SchemaMigrationCommandPreview preview_schema_migration_commands(
        const Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema,
        const SchemaMigrationPolicy& policy)
    {
        try {
            SchemaMigrationCommandPreview preview;

            // This function deliberately does not create GraphCommandRecord or
            // GraphCommandBatch values. No graph mutation has happened, so it only
            // reports prospective operations for a later apply layer.
            preview.apply_preview = preview_schema_migration_application(
                graph,
                source_schema,
                target_schema,
                policy);

            if (preview.apply_preview.status == SchemaMigrationApplyPreviewStatus::PolicyInvalid) {
                preview.result = preview.apply_preview.result;
                preview.status = SchemaMigrationCommandPreviewStatus::PolicyInvalid;
                return preview;
            }

            if (preview.apply_preview.status == SchemaMigrationApplyPreviewStatus::PlanFailed) {
                preview.result = preview.apply_preview.result;
                preview.status = SchemaMigrationCommandPreviewStatus::PlanFailed;
                return preview;
            }

            if (!preview.apply_preview.uncovered_blocking_actions.empty()) {
                preview.result = Result::Ok;
                preview.status = SchemaMigrationCommandPreviewStatus::BlockedByUncoveredActions;
                return preview;
            }

            // Covered actions can produce preview steps, but they are still only
            // descriptions. The graph, schemas, and policy remain unchanged here.
            append_preview_steps(graph, preview.apply_preview, policy, preview.steps);

            preview.result = Result::Ok;
            if (!preview.steps.empty() || preview.apply_preview.ready()) {
                preview.status = SchemaMigrationCommandPreviewStatus::Ready;
            } else {
                preview.status = SchemaMigrationCommandPreviewStatus::NoPreviewableOperations;
            }

            return preview;
        } catch (const std::bad_alloc&) {
            return preview_failure(
                Result::AllocationFailure,
                SchemaMigrationCommandPreviewStatus::PlanFailed);
        }
    }
}
