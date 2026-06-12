// Implements atomic, non-destructive schema migration application.
// Graph state is rewritten through an in-memory DTO working copy and the input
// graph is replaced only after import, target validation, and diffing succeed.

#include <wng/schema_migration_apply.hpp>

#include <cstdint>
#include <limits>
#include <new>
#include <utility>

#include <wng/graph.hpp>
#include <wng/graph_validation.hpp>
#include <wng/schema.hpp>
#include <wng/serialization.hpp>
#include <wng/serialization_dto.hpp>

namespace
{
    wng::SchemaMigrationApplyResult apply_failure(
        wng::Result result,
        wng::SchemaMigrationApplyStatus status)
    {
        wng::SchemaMigrationApplyResult apply_result;
        apply_result.result = result;
        apply_result.status = status;
        return apply_result;
    }

    wng::Result first_error_result(const wng::ValidationReport& report)
    {
        for (const wng::ValidationIssue& issue : report.issues) {
            if (issue.severity == wng::ValidationSeverity::Error) {
                return issue.result;
            }
        }

        return wng::Result::Ok;
    }

    wng::NodeDto* find_node_dto(wng::GraphDto& dto, wng::NodeId id)
    {
        for (wng::NodeDto& node : dto.nodes) {
            if (node.id == id) {
                return &node;
            }
        }

        return nullptr;
    }

    wng::PortDto* find_port_dto(wng::GraphDto& dto, wng::PortId id)
    {
        for (wng::PortDto& port : dto.ports) {
            if (port.id == id) {
                return &port;
            }
        }

        return nullptr;
    }

    const wng::PortDefinition* find_port_definition(
        const wng::GraphSchema& schema,
        const wng::PortDefinitionIdentity& identity)
    {
        const wng::NodeDefinition* node_definition =
            schema.find_node_definition(identity.node_type);
        if (node_definition == nullptr) {
            return nullptr;
        }

        const std::vector<wng::PortDefinition>& ports =
            identity.kind == wng::PortKind::Input ?
                node_definition->inputs :
                node_definition->outputs;
        for (const wng::PortDefinition& port : ports) {
            if (port.kind == identity.kind && port.name == identity.name) {
                return &port;
            }
        }

        return nullptr;
    }

    wng::PortId next_port_id(const wng::GraphDto& dto)
    {
        std::uint32_t max_id = 0U;
        for (const wng::PortDto& port : dto.ports) {
            if (port.id.value > max_id) {
                max_id = port.id.value;
            }
        }

        if (max_id >= std::numeric_limits<std::uint32_t>::max() - 1U) {
            return {};
        }

        return wng::PortId { max_id + 1U };
    }

    wng::Result increment_port_id(wng::PortId& id)
    {
        if (id.value >= std::numeric_limits<std::uint32_t>::max() - 1U) {
            return wng::Result::InvalidArgument;
        }

        ++id.value;
        return wng::Result::Ok;
    }

    bool node_has_matching_port(
        const wng::GraphDto& dto,
        wng::NodeId node,
        const wng::PortDefinitionIdentity& identity)
    {
        for (const wng::PortDto& port : dto.ports) {
            if (port.node == node &&
                port.kind == identity.kind &&
                port.name == identity.name) {
                return true;
            }
        }

        return false;
    }

    wng::Result append_port_to_node(
        wng::NodeDto& node,
        wng::PortId port,
        wng::PortKind kind)
    {
        if (kind == wng::PortKind::Input) {
            node.inputs.push_back(port);
            return wng::Result::Ok;
        }

        if (kind == wng::PortKind::Output) {
            node.outputs.push_back(port);
            return wng::Result::Ok;
        }

        return wng::Result::InvalidArgument;
    }

    bool contains_destructive_step(const wng::SchemaMigrationCommandPreview& preview)
    {
        for (const wng::SchemaMigrationCommandPreviewStep& step : preview.steps) {
            if (step.destructive) {
                return true;
            }
        }

        return false;
    }

    wng::Result apply_rename_node_type_step(
        wng::GraphDto& dto,
        const wng::SchemaMigrationCommandPreviewStep& step)
    {
        for (wng::NodeId node_id : step.affected_nodes) {
            wng::NodeDto* node = find_node_dto(dto, node_id);
            if (node == nullptr) {
                return wng::Result::NotFound;
            }

            if (node->type != step.from_node_type) {
                return wng::Result::InvalidArgument;
            }

            node->type = step.to_node_type;
        }

        return wng::Result::Ok;
    }

    wng::Result apply_rename_port_definition_step(
        wng::GraphDto& dto,
        const wng::SchemaMigrationCommandPreviewStep& step)
    {
        if (step.from_port.kind != step.to_port.kind) {
            return wng::Result::InvalidArgument;
        }

        for (wng::PortId port_id : step.affected_ports) {
            wng::PortDto* port = find_port_dto(dto, port_id);
            if (port == nullptr) {
                return wng::Result::NotFound;
            }

            const wng::NodeDto* owner = find_node_dto(dto, port->node);
            if (owner == nullptr) {
                return wng::Result::NotFound;
            }

            if (owner->type != step.from_port.node_type &&
                owner->type != step.to_port.node_type) {
                return wng::Result::InvalidArgument;
            }

            if (port->kind != step.from_port.kind || port->name != step.from_port.name) {
                return wng::Result::InvalidArgument;
            }

            port->name = step.to_port.name;
        }

        return wng::Result::Ok;
    }

    wng::Result apply_change_port_type_step(
        wng::GraphDto& dto,
        const wng::SchemaMigrationCommandPreviewStep& step)
    {
        for (wng::PortId port_id : step.affected_ports) {
            wng::PortDto* port = find_port_dto(dto, port_id);
            if (port == nullptr) {
                return wng::Result::NotFound;
            }

            if (port->type != step.from_type) {
                return wng::Result::InvalidArgument;
            }

            port->type = step.to_type;
        }

        return wng::Result::Ok;
    }

    wng::Result apply_add_required_port_step(
        wng::GraphDto& dto,
        const wng::GraphSchema& target_schema,
        const wng::SchemaMigrationCommandPreviewStep& step)
    {
        const wng::PortDefinition* definition =
            find_port_definition(target_schema, step.to_port);
        if (definition == nullptr) {
            return wng::Result::NotFound;
        }

        if (!definition->required) {
            return wng::Result::InvalidArgument;
        }

        wng::PortId next_id = next_port_id(dto);
        if (next_id.value == 0U) {
            return wng::Result::InvalidArgument;
        }

        for (wng::NodeId node_id : step.affected_nodes) {
            wng::NodeDto* node = find_node_dto(dto, node_id);
            if (node == nullptr) {
                return wng::Result::NotFound;
            }

            if (node->type != step.to_port.node_type) {
                return wng::Result::InvalidArgument;
            }

            if (node_has_matching_port(dto, node->id, step.to_port)) {
                continue;
            }

            wng::PortDto port;
            port.id = next_id;
            port.node = node->id;
            port.kind = definition->kind;
            port.name = definition->name;
            port.type = definition->type;
            port.visible = definition->visible;
            port.enabled = definition->enabled;

            // WNG has no runtime value system yet. The policy default remains
            // diagnostic/result metadata and is not stored on the created port.
            const wng::Result append_result = append_port_to_node(
                *node,
                port.id,
                port.kind);
            if (append_result != wng::Result::Ok) {
                return append_result;
            }

            dto.ports.push_back(port);

            const wng::Result increment_result = increment_port_id(next_id);
            if (increment_result != wng::Result::Ok) {
                return increment_result;
            }
        }

        return wng::Result::Ok;
    }

    wng::Result apply_step(
        wng::GraphDto& dto,
        const wng::GraphSchema& target_schema,
        const wng::SchemaMigrationCommandPreviewStep& step)
    {
        switch (step.kind) {
        case wng::SchemaMigrationCommandPreviewStepKind::RenameNodeType:
            return apply_rename_node_type_step(dto, step);
        case wng::SchemaMigrationCommandPreviewStepKind::RenamePortDefinition:
            return apply_rename_port_definition_step(dto, step);
        case wng::SchemaMigrationCommandPreviewStepKind::ChangePortType:
            return apply_change_port_type_step(dto, step);
        case wng::SchemaMigrationCommandPreviewStepKind::AddRequiredPort:
            return apply_add_required_port_step(dto, target_schema, step);
        case wng::SchemaMigrationCommandPreviewStepKind::RemoveNodesForRemovedType:
        case wng::SchemaMigrationCommandPreviewStepKind::RemovePortsForRemovedDefinition:
            return wng::Result::InvalidArgument;
        }

        return wng::Result::InvalidArgument;
    }

    wng::SchemaMigrationApplyResult no_change_success(
        const wng::Graph& graph,
        wng::SchemaMigrationCommandPreview command_preview)
    {
        wng::SchemaMigrationApplyResult result;
        result.result = wng::Result::Ok;
        result.status = wng::SchemaMigrationApplyStatus::Applied;
        result.command_preview = std::move(command_preview);
        result.diff = wng::diff_graphs(graph, graph);
        return result;
    }
}

namespace wng
{
    bool SchemaMigrationApplyResult::success() const
    {
        return result == Result::Ok;
    }

    bool SchemaMigrationApplyResult::applied() const
    {
        return result == Result::Ok && status == SchemaMigrationApplyStatus::Applied;
    }

    SchemaMigrationApplyResult apply_schema_migration(
        Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema,
        const SchemaMigrationPolicy& policy)
    {
        try {
            SchemaMigrationApplyResult result;

            // Command preview is speculative and does not create GraphCommandRecord
            // or GraphCommandBatch values. Apply consumes the preview, mutates only
            // a DTO working copy, and keeps the source graph untouched on failure.
            result.command_preview = preview_schema_migration_commands(
                graph,
                source_schema,
                target_schema,
                policy);

            if (result.command_preview.status == SchemaMigrationCommandPreviewStatus::PolicyInvalid ||
                result.command_preview.status == SchemaMigrationCommandPreviewStatus::PlanFailed) {
                result.result = result.command_preview.result;
                result.status = SchemaMigrationApplyStatus::PreviewFailed;
                return result;
            }

            if (result.command_preview.status ==
                SchemaMigrationCommandPreviewStatus::BlockedByUncoveredActions) {
                result.result = Result::InvalidArgument;
                result.status = SchemaMigrationApplyStatus::NotReady;
                return result;
            }

            if (result.command_preview.status ==
                SchemaMigrationCommandPreviewStatus::NoPreviewableOperations) {
                const ValidationReport report = validate_graph(graph, target_schema);
                if (report.valid()) {
                    return no_change_success(graph, std::move(result.command_preview));
                }

                result.result = first_error_result(report);
                result.status = SchemaMigrationApplyStatus::NotReady;
                return result;
            }

            if (result.command_preview.status != SchemaMigrationCommandPreviewStatus::Ready) {
                result.result = Result::InvalidArgument;
                result.status = SchemaMigrationApplyStatus::NotReady;
                return result;
            }

            // Destructive previews are explicitly rejected before any working-copy
            // mutation. A later destructive-apply patch can define removal policy,
            // command history behavior, and user-facing consequences.
            if (contains_destructive_step(result.command_preview)) {
                result.result = Result::InvalidArgument;
                result.status = SchemaMigrationApplyStatus::UnsupportedDestructiveOperation;
                return result;
            }

            if (result.command_preview.steps.empty()) {
                return no_change_success(graph, std::move(result.command_preview));
            }

            GraphDto working_dto;
            const Result export_result = export_graph(graph, &working_dto);
            if (export_result != Result::Ok) {
                result.result = export_result;
                result.status = SchemaMigrationApplyStatus::NotReady;
                return result;
            }

            for (const SchemaMigrationCommandPreviewStep& step : result.command_preview.steps) {
                const Result step_result = apply_step(working_dto, target_schema, step);
                if (step_result != Result::Ok) {
                    result.result = step_result;
                    result.status = SchemaMigrationApplyStatus::NotReady;
                    result.applied_steps.clear();
                    return result;
                }

                SchemaMigrationAppliedStep applied_step;
                applied_step.preview_step = step;
                applied_step.applied = true;
                result.applied_steps.push_back(applied_step);
            }

            Graph replacement;
            const Result import_result = import_graph(working_dto, &replacement);
            if (import_result != Result::Ok) {
                result.result = import_result;
                result.status = SchemaMigrationApplyStatus::TargetValidationFailed;
                result.applied_steps.clear();
                return result;
            }

            const ValidationReport report = validate_graph(replacement, target_schema);
            if (!report.valid()) {
                result.result = first_error_result(report);
                result.status = SchemaMigrationApplyStatus::TargetValidationFailed;
                result.applied_steps.clear();
                return result;
            }

            GraphDiff diff = diff_graphs(graph, replacement);
            if (!diff.success()) {
                result.result = diff.result;
                result.status = SchemaMigrationApplyStatus::NotReady;
                result.applied_steps.clear();
                return result;
            }

            // This assignment is the only input graph mutation and happens only
            // after all DTO rewrites, import, target validation, and diffing pass.
            graph = replacement;

            result.result = Result::Ok;
            result.status = SchemaMigrationApplyStatus::Applied;
            result.diff = std::move(diff);
            return result;
        } catch (const std::bad_alloc&) {
            return apply_failure(
                Result::AllocationFailure,
                SchemaMigrationApplyStatus::PreviewFailed);
        }
    }
}
