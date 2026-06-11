// Implements deterministic schema comparison for GraphSchema and SchemaSnapshot.
// The diff is structural and diagnostic only; it does not apply patches, migrate
// schemas, persist schemas, or define merge/conflict policy.

#include <new>
#include <string>
#include <vector>

#include <wng/schema_diff.hpp>

namespace
{
    wng::SchemaDiff schema_diff_failure(wng::Result result)
    {
        wng::SchemaDiff diff;
        diff.result = result;
        return diff;
    }

    const wng::NodeDefinition* find_node_definition_by_type(
        const std::vector<wng::NodeDefinition>& definitions,
        const std::string& type)
    {
        for (const wng::NodeDefinition& definition : definitions) {
            if (definition.type == type) {
                return &definition;
            }
        }

        return nullptr;
    }

    const wng::PortDefinition* find_port_definition(
        const wng::NodeDefinition& definition,
        wng::PortKind kind,
        const std::string& name)
    {
        const std::vector<wng::PortDefinition>& ports =
            kind == wng::PortKind::Input ? definition.inputs : definition.outputs;

        for (const wng::PortDefinition& port : ports) {
            if (port.kind == kind && port.name == name) {
                return &port;
            }
        }

        return nullptr;
    }

    bool node_definition_scalar_equal(
        const wng::NodeDefinition& a,
        const wng::NodeDefinition& b)
    {
        return a.type == b.type &&
            a.display_name == b.display_name &&
            a.visible == b.visible &&
            a.enabled == b.enabled;
    }

    bool port_definition_equal(
        const wng::PortDefinition& a,
        const wng::PortDefinition& b)
    {
        return a.name == b.name &&
            a.kind == b.kind &&
            a.type == b.type &&
            a.required == b.required &&
            a.visible == b.visible &&
            a.enabled == b.enabled;
    }

    void append_node_definition_diffs(
        const wng::GraphSchema& before,
        const wng::GraphSchema& after,
        wng::SchemaDiff& diff)
    {
        const std::vector<wng::NodeDefinition>& before_nodes = before.node_definitions();
        const std::vector<wng::NodeDefinition>& after_nodes = after.node_definitions();

        // Ordering is part of the API contract: removed definitions follow the
        // before schema order, added definitions follow the after schema order,
        // and modifications return to before schema order for reproducibility.
        for (const wng::NodeDefinition& before_node : before_nodes) {
            if (find_node_definition_by_type(after_nodes, before_node.type) == nullptr) {
                wng::NodeDefinitionDiff node_diff;
                node_diff.change = wng::SchemaDiffChange::Removed;
                node_diff.type = before_node.type;
                node_diff.before = before_node;
                diff.nodes.push_back(node_diff);
            }
        }

        for (const wng::NodeDefinition& after_node : after_nodes) {
            if (find_node_definition_by_type(before_nodes, after_node.type) == nullptr) {
                wng::NodeDefinitionDiff node_diff;
                node_diff.change = wng::SchemaDiffChange::Added;
                node_diff.type = after_node.type;
                node_diff.after = after_node;
                diff.nodes.push_back(node_diff);
            }
        }

        for (const wng::NodeDefinition& before_node : before_nodes) {
            const wng::NodeDefinition* after_node =
                find_node_definition_by_type(after_nodes, before_node.type);
            if (after_node != nullptr &&
                !node_definition_scalar_equal(before_node, *after_node)) {
                wng::NodeDefinitionDiff node_diff;
                node_diff.change = wng::SchemaDiffChange::Modified;
                node_diff.type = before_node.type;
                node_diff.before = before_node;
                node_diff.after = *after_node;
                diff.nodes.push_back(node_diff);
            }
        }
    }

    void append_removed_port_diffs(
        const wng::GraphSchema& before,
        const wng::GraphSchema& after,
        wng::SchemaDiff& diff)
    {
        for (const wng::NodeDefinition& before_node : before.node_definitions()) {
            const wng::NodeDefinition* after_node =
                after.find_node_definition(before_node.type);

            for (const wng::PortDefinition& port : before_node.inputs) {
                if (after_node == nullptr ||
                    find_port_definition(*after_node, port.kind, port.name) == nullptr) {
                    wng::PortDefinitionDiff port_diff;
                    port_diff.change = wng::SchemaDiffChange::Removed;
                    port_diff.node_type = before_node.type;
                    port_diff.kind = port.kind;
                    port_diff.name = port.name;
                    port_diff.before = port;
                    diff.ports.push_back(port_diff);
                }
            }

            for (const wng::PortDefinition& port : before_node.outputs) {
                if (after_node == nullptr ||
                    find_port_definition(*after_node, port.kind, port.name) == nullptr) {
                    wng::PortDefinitionDiff port_diff;
                    port_diff.change = wng::SchemaDiffChange::Removed;
                    port_diff.node_type = before_node.type;
                    port_diff.kind = port.kind;
                    port_diff.name = port.name;
                    port_diff.before = port;
                    diff.ports.push_back(port_diff);
                }
            }
        }
    }

    void append_added_port_diffs(
        const wng::GraphSchema& before,
        const wng::GraphSchema& after,
        wng::SchemaDiff& diff)
    {
        for (const wng::NodeDefinition& after_node : after.node_definitions()) {
            const wng::NodeDefinition* before_node =
                before.find_node_definition(after_node.type);

            for (const wng::PortDefinition& port : after_node.inputs) {
                if (before_node == nullptr ||
                    find_port_definition(*before_node, port.kind, port.name) == nullptr) {
                    wng::PortDefinitionDiff port_diff;
                    port_diff.change = wng::SchemaDiffChange::Added;
                    port_diff.node_type = after_node.type;
                    port_diff.kind = port.kind;
                    port_diff.name = port.name;
                    port_diff.after = port;
                    diff.ports.push_back(port_diff);
                }
            }

            for (const wng::PortDefinition& port : after_node.outputs) {
                if (before_node == nullptr ||
                    find_port_definition(*before_node, port.kind, port.name) == nullptr) {
                    wng::PortDefinitionDiff port_diff;
                    port_diff.change = wng::SchemaDiffChange::Added;
                    port_diff.node_type = after_node.type;
                    port_diff.kind = port.kind;
                    port_diff.name = port.name;
                    port_diff.after = port;
                    diff.ports.push_back(port_diff);
                }
            }
        }
    }

    void append_modified_port_diffs(
        const wng::GraphSchema& before,
        const wng::GraphSchema& after,
        wng::SchemaDiff& diff)
    {
        for (const wng::NodeDefinition& before_node : before.node_definitions()) {
            const wng::NodeDefinition* after_node =
                after.find_node_definition(before_node.type);
            if (after_node == nullptr) {
                continue;
            }

            for (const wng::PortDefinition& before_port : before_node.inputs) {
                const wng::PortDefinition* after_port =
                    find_port_definition(*after_node, before_port.kind, before_port.name);
                if (after_port != nullptr && !port_definition_equal(before_port, *after_port)) {
                    wng::PortDefinitionDiff port_diff;
                    port_diff.change = wng::SchemaDiffChange::Modified;
                    port_diff.node_type = before_node.type;
                    port_diff.kind = before_port.kind;
                    port_diff.name = before_port.name;
                    port_diff.before = before_port;
                    port_diff.after = *after_port;
                    diff.ports.push_back(port_diff);
                }
            }

            for (const wng::PortDefinition& before_port : before_node.outputs) {
                const wng::PortDefinition* after_port =
                    find_port_definition(*after_node, before_port.kind, before_port.name);
                if (after_port != nullptr && !port_definition_equal(before_port, *after_port)) {
                    wng::PortDefinitionDiff port_diff;
                    port_diff.change = wng::SchemaDiffChange::Modified;
                    port_diff.node_type = before_node.type;
                    port_diff.kind = before_port.kind;
                    port_diff.name = before_port.name;
                    port_diff.before = before_port;
                    port_diff.after = *after_port;
                    diff.ports.push_back(port_diff);
                }
            }
        }
    }

    void append_port_definition_diffs(
        const wng::GraphSchema& before,
        const wng::GraphSchema& after,
        wng::SchemaDiff& diff)
    {
        // Port definition identity is stable across type/required/enabled changes:
        // owning node type + port kind + port name. This keeps a port type change
        // as one modification rather than a remove/add pair.
        append_removed_port_diffs(before, after, diff);
        append_added_port_diffs(before, after, diff);
        append_modified_port_diffs(before, after, diff);
    }
}

namespace wng
{
    bool SchemaDiff::empty() const
    {
        return nodes.empty() && ports.empty();
    }

    bool SchemaDiff::changed() const
    {
        return !empty();
    }

    bool SchemaDiff::success() const
    {
        return result == Result::Ok;
    }

    SchemaDiff diff_schemas(
        const GraphSchema& before,
        const GraphSchema& after)
    {
        try {
            SchemaDiff diff;
            append_node_definition_diffs(before, after, diff);
            append_port_definition_diffs(before, after, diff);
            return diff;
        } catch (const std::bad_alloc&) {
            return schema_diff_failure(Result::AllocationFailure);
        }
    }

    SchemaDiff diff_schema_snapshots(
        const SchemaSnapshot& before,
        const SchemaSnapshot& after)
    {
        try {
            // Snapshot diff restores through SchemaSnapshot APIs before comparing.
            // That keeps snapshot validation centralized and leaves SchemaDiff as
            // the single source of schema comparison semantics.
            GraphSchema before_schema;
            const Result before_result = restore_schema_snapshot(before_schema, before);
            if (before_result != Result::Ok) {
                return schema_diff_failure(before_result);
            }

            GraphSchema after_schema;
            const Result after_result = restore_schema_snapshot(after_schema, after);
            if (after_result != Result::Ok) {
                return schema_diff_failure(after_result);
            }

            return diff_schemas(before_schema, after_schema);
        } catch (const std::bad_alloc&) {
            return schema_diff_failure(Result::AllocationFailure);
        }
    }
}
