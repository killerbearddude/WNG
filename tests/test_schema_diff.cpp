// Exercises deterministic GraphSchema and SchemaSnapshot diffing.
// Tests focus on stable identity matching, deterministic ordering, snapshot
// delegation, and non-mutation guarantees needed by future migration tooling.

#include <cassert>
#include <string>
#include <vector>

#include <wng/graph.hpp>
#include <wng/schema_diff.hpp>
#include <wng/schema_mutation.hpp>

namespace
{
    wng::PortDefinition input_definition(
        const std::string& name,
        const std::string& type = "number")
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Input;
        definition.type = type;
        return definition;
    }

    wng::PortDefinition output_definition(
        const std::string& name,
        const std::string& type = "number")
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Output;
        definition.type = type;
        return definition;
    }

    wng::NodeDefinition make_definition(
        const std::string& type,
        const std::string& display_name)
    {
        wng::NodeDefinition definition;
        definition.type = type;
        definition.display_name = display_name;
        return definition;
    }

    wng::NodeDefinition make_add_definition()
    {
        wng::NodeDefinition definition = make_definition("math.add", "Add");
        definition.inputs.push_back(input_definition("a"));
        definition.inputs.push_back(input_definition("b"));
        definition.outputs.push_back(output_definition("result"));
        return definition;
    }

    wng::GraphSchema schema_with(const wng::NodeDefinition& definition)
    {
        wng::GraphSchema schema;
        assert(schema.add_node_definition(definition) == wng::Result::Ok);
        return schema;
    }

    wng::GraphSchema schema_with(const std::vector<wng::NodeDefinition>& definitions)
    {
        wng::GraphSchema schema;
        for (const wng::NodeDefinition& definition : definitions) {
            assert(schema.add_node_definition(definition) == wng::Result::Ok);
        }
        return schema;
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

    bool port_vectors_equal(
        const std::vector<wng::PortDefinition>& a,
        const std::vector<wng::PortDefinition>& b)
    {
        if (a.size() != b.size()) {
            return false;
        }

        for (std::size_t i = 0; i < a.size(); ++i) {
            if (!port_definition_equal(a[i], b[i])) {
                return false;
            }
        }

        return true;
    }

    bool node_definition_equal(
        const wng::NodeDefinition& a,
        const wng::NodeDefinition& b)
    {
        return a.type == b.type &&
            a.display_name == b.display_name &&
            a.visible == b.visible &&
            a.enabled == b.enabled &&
            port_vectors_equal(a.inputs, b.inputs) &&
            port_vectors_equal(a.outputs, b.outputs);
    }

    bool node_vectors_equal(
        const std::vector<wng::NodeDefinition>& a,
        const std::vector<wng::NodeDefinition>& b)
    {
        if (a.size() != b.size()) {
            return false;
        }

        for (std::size_t i = 0; i < a.size(); ++i) {
            if (!node_definition_equal(a[i], b[i])) {
                return false;
            }
        }

        return true;
    }

    void assert_schema_unchanged(
        const wng::GraphSchema& schema,
        const std::vector<wng::NodeDefinition>& before)
    {
        assert(node_vectors_equal(schema.node_definitions(), before));
    }

    wng::SchemaSnapshot capture_snapshot(const wng::GraphSchema& schema)
    {
        const wng::SchemaSnapshotResult result = wng::capture_schema_snapshot(schema);
        assert(result.result == wng::Result::Ok);
        return result.snapshot;
    }

    wng::NodeDesc make_desc(const std::string& type)
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = type;
        return desc;
    }
}

int main()
{
    {
        // Empty schemas should compare cleanly. This gives callers a reliable
        // baseline for editor drafts and migration tests that start with no definitions.
        const wng::GraphSchema before;
        const wng::GraphSchema after;

        const wng::SchemaDiff diff = wng::diff_schemas(before, after);
        assert(diff.result == wng::Result::Ok);
        assert(diff.success());
        assert(diff.empty());
        assert(!diff.changed());
        assert(diff.nodes.empty());
        assert(diff.ports.empty());
    }

    {
        // Equivalent schemas should produce no changes even when they are distinct
        // objects. Future regression tests can compare copied schemas without noise.
        const wng::GraphSchema before = schema_with(make_add_definition());
        const wng::GraphSchema after = schema_with(make_add_definition());

        const wng::SchemaDiff diff = wng::diff_schemas(before, after);
        assert(diff.result == wng::Result::Ok);
        assert(diff.empty());
    }

    {
        // Added node definitions are matched by stable node type, not by storage
        // index or display name.
        const wng::GraphSchema before;
        const wng::GraphSchema after = schema_with(make_definition("debug.print", "Print"));

        const wng::SchemaDiff diff = wng::diff_schemas(before, after);
        assert(diff.result == wng::Result::Ok);
        assert(diff.nodes.size() == 1);
        assert(diff.nodes[0].change == wng::SchemaDiffChange::Added);
        assert(diff.nodes[0].type == "debug.print");
        assert(diff.nodes[0].after.type == "debug.print");
    }

    {
        // Removed node definitions are reported in the source schema identity space
        // so migration diagnostics can explain what disappeared.
        const wng::GraphSchema before = schema_with(make_definition("debug.print", "Print"));
        const wng::GraphSchema after;

        const wng::SchemaDiff diff = wng::diff_schemas(before, after);
        assert(diff.result == wng::Result::Ok);
        assert(diff.nodes.size() == 1);
        assert(diff.nodes[0].change == wng::SchemaDiffChange::Removed);
        assert(diff.nodes[0].type == "debug.print");
        assert(diff.nodes[0].before.type == "debug.print");
    }

    {
        // Scalar node-definition changes are reported separately from port changes.
        // This avoids double-reporting a port edit as an owning-node modification.
        wng::NodeDefinition before_definition = make_add_definition();
        wng::NodeDefinition after_definition = before_definition;
        after_definition.display_name = "Add Numbers";
        after_definition.enabled = false;

        const wng::SchemaDiff diff = wng::diff_schemas(
            schema_with(before_definition),
            schema_with(after_definition));
        assert(diff.result == wng::Result::Ok);
        assert(diff.nodes.size() == 1);
        assert(diff.nodes[0].change == wng::SchemaDiffChange::Modified);
        assert(diff.nodes[0].type == "math.add");
        assert(diff.nodes[0].before.display_name == "Add");
        assert(diff.nodes[0].after.display_name == "Add Numbers");
        assert(diff.nodes[0].after.enabled == false);
    }

    {
        // Added ports are identified by owning node type, port kind, and name.
        // Adding a port should not require the owning node type to be re-identified.
        wng::NodeDefinition before_definition = make_add_definition();
        wng::NodeDefinition after_definition = before_definition;
        after_definition.inputs.push_back(input_definition("scale"));

        const wng::SchemaDiff diff = wng::diff_schemas(
            schema_with(before_definition),
            schema_with(after_definition));
        assert(diff.result == wng::Result::Ok);
        assert(diff.ports.size() == 1);
        assert(diff.ports[0].change == wng::SchemaDiffChange::Added);
        assert(diff.ports[0].node_type == "math.add");
        assert(diff.ports[0].kind == wng::PortKind::Input);
        assert(diff.ports[0].name == "scale");
    }

    {
        // Removed ports remain visible as port diffs rather than being hidden behind
        // a generic node-definition change.
        wng::NodeDefinition before_definition = make_add_definition();
        wng::NodeDefinition after_definition = before_definition;
        after_definition.inputs.pop_back();

        const wng::SchemaDiff diff = wng::diff_schemas(
            schema_with(before_definition),
            schema_with(after_definition));
        assert(diff.result == wng::Result::Ok);
        assert(diff.ports.size() == 1);
        assert(diff.ports[0].change == wng::SchemaDiffChange::Removed);
        assert(diff.ports[0].node_type == "math.add");
        assert(diff.ports[0].name == "b");
    }

    {
        // Port definition field changes should be modifications under the stable
        // port identity rule. Migration tooling depends on this not becoming a
        // remove/add pair.
        wng::NodeDefinition before_definition = make_add_definition();
        wng::NodeDefinition after_definition = before_definition;
        after_definition.inputs[0].required = true;
        after_definition.inputs[0].enabled = false;

        const wng::SchemaDiff diff = wng::diff_schemas(
            schema_with(before_definition),
            schema_with(after_definition));
        assert(diff.result == wng::Result::Ok);
        assert(diff.ports.size() == 1);
        assert(diff.ports[0].change == wng::SchemaDiffChange::Modified);
        assert(diff.ports[0].node_type == "math.add");
        assert(diff.ports[0].name == "a");
        assert(diff.ports[0].before.required == false);
        assert(diff.ports[0].after.required == true);
        assert(diff.ports[0].after.enabled == false);
    }

    {
        // A port type change keeps the same identity and is therefore a single
        // modification, not a removed number port plus an added string port.
        wng::NodeDefinition before_definition = make_add_definition();
        wng::NodeDefinition after_definition = before_definition;
        after_definition.inputs[0].type = "string";

        const wng::SchemaDiff diff = wng::diff_schemas(
            schema_with(before_definition),
            schema_with(after_definition));
        assert(diff.result == wng::Result::Ok);
        assert(diff.ports.size() == 1);
        assert(diff.ports[0].change == wng::SchemaDiffChange::Modified);
        assert(diff.ports[0].before.type == "number");
        assert(diff.ports[0].after.type == "string");
    }

    {
        // Node diff ordering is deterministic: removals in before order, additions
        // in after order, then modifications in before order.
        wng::NodeDefinition remove_a = make_definition("remove.a", "Remove A");
        wng::NodeDefinition modify_a = make_definition("modify.a", "Modify A");
        wng::NodeDefinition remove_b = make_definition("remove.b", "Remove B");
        wng::NodeDefinition modify_b = make_definition("modify.b", "Modify B");

        wng::NodeDefinition modified_a = modify_a;
        modified_a.display_name = "Modified A";
        wng::NodeDefinition modified_b = modify_b;
        modified_b.display_name = "Modified B";

        const wng::GraphSchema before = schema_with({ remove_a, modify_a, remove_b, modify_b });
        const wng::GraphSchema after = schema_with({
            make_definition("add.a", "Add A"),
            modified_a,
            make_definition("add.b", "Add B"),
            modified_b
        });

        const wng::SchemaDiff diff = wng::diff_schemas(before, after);
        assert(diff.result == wng::Result::Ok);
        assert(diff.nodes.size() == 6);
        assert(diff.nodes[0].change == wng::SchemaDiffChange::Removed);
        assert(diff.nodes[0].type == "remove.a");
        assert(diff.nodes[1].change == wng::SchemaDiffChange::Removed);
        assert(diff.nodes[1].type == "remove.b");
        assert(diff.nodes[2].change == wng::SchemaDiffChange::Added);
        assert(diff.nodes[2].type == "add.a");
        assert(diff.nodes[3].change == wng::SchemaDiffChange::Added);
        assert(diff.nodes[3].type == "add.b");
        assert(diff.nodes[4].change == wng::SchemaDiffChange::Modified);
        assert(diff.nodes[4].type == "modify.a");
        assert(diff.nodes[5].change == wng::SchemaDiffChange::Modified);
        assert(diff.nodes[5].type == "modify.b");
    }

    {
        // Port diff ordering mirrors schema construction order: removed ports in
        // before node/port order, added ports in after node/port order, then
        // modified ports in before node/port order.
        wng::NodeDefinition before_first = make_definition("first", "First");
        before_first.inputs.push_back(input_definition("remove_a"));
        before_first.inputs.push_back(input_definition("modify_a"));
        before_first.outputs.push_back(output_definition("remove_b"));
        before_first.outputs.push_back(output_definition("modify_b"));

        wng::NodeDefinition after_first = make_definition("first", "First");
        after_first.inputs.push_back(input_definition("add_a"));
        after_first.inputs.push_back(input_definition("modify_a", "string"));
        after_first.outputs.push_back(output_definition("add_b"));
        after_first.outputs.push_back(output_definition("modify_b", "string"));

        const wng::SchemaDiff diff = wng::diff_schemas(
            schema_with(before_first),
            schema_with(after_first));
        assert(diff.result == wng::Result::Ok);
        assert(diff.ports.size() == 6);
        assert(diff.ports[0].change == wng::SchemaDiffChange::Removed);
        assert(diff.ports[0].name == "remove_a");
        assert(diff.ports[1].change == wng::SchemaDiffChange::Removed);
        assert(diff.ports[1].name == "remove_b");
        assert(diff.ports[2].change == wng::SchemaDiffChange::Added);
        assert(diff.ports[2].name == "add_a");
        assert(diff.ports[3].change == wng::SchemaDiffChange::Added);
        assert(diff.ports[3].name == "add_b");
        assert(diff.ports[4].change == wng::SchemaDiffChange::Modified);
        assert(diff.ports[4].name == "modify_a");
        assert(diff.ports[5].change == wng::SchemaDiffChange::Modified);
        assert(diff.ports[5].name == "modify_b");
    }

    {
        // Equivalent snapshots should compare empty after restore. Snapshot diffing
        // delegates through schema restore rather than comparing vectors directly.
        const wng::GraphSchema schema = schema_with(make_add_definition());
        const wng::SchemaSnapshot a = capture_snapshot(schema);
        const wng::SchemaSnapshot b = capture_snapshot(schema);

        const wng::SchemaDiff diff = wng::diff_schema_snapshots(a, b);
        assert(diff.result == wng::Result::Ok);
        assert(diff.empty());
    }

    {
        // Snapshot-to-snapshot comparison should report added node definitions with
        // the same semantics as direct schema comparison.
        const wng::GraphSchema before_schema;
        const wng::GraphSchema after_schema = schema_with(make_definition("debug.print", "Print"));

        const wng::SchemaDiff diff = wng::diff_schema_snapshots(
            capture_snapshot(before_schema),
            capture_snapshot(after_schema));
        assert(diff.result == wng::Result::Ok);
        assert(diff.nodes.size() == 1);
        assert(diff.nodes[0].change == wng::SchemaDiffChange::Added);
        assert(diff.nodes[0].type == "debug.print");
    }

    {
        // Snapshot diffing should preserve the stable port identity rule for field
        // changes after snapshots have been restored to temporary schemas.
        wng::NodeDefinition before_definition = make_add_definition();
        wng::NodeDefinition after_definition = before_definition;
        after_definition.outputs[0].type = "string";

        const wng::SchemaDiff diff = wng::diff_schema_snapshots(
            capture_snapshot(schema_with(before_definition)),
            capture_snapshot(schema_with(after_definition)));
        assert(diff.result == wng::Result::Ok);
        assert(diff.ports.size() == 1);
        assert(diff.ports[0].change == wng::SchemaDiffChange::Modified);
        assert(diff.ports[0].kind == wng::PortKind::Output);
        assert(diff.ports[0].name == "result");
    }

    {
        // Direct schema diffing must not mutate either input schema. Callers can
        // safely use it for diagnostics inside validation and planning workflows.
        const wng::GraphSchema before = schema_with(make_add_definition());
        wng::NodeDefinition after_definition = make_add_definition();
        after_definition.display_name = "Changed";
        const wng::GraphSchema after = schema_with(after_definition);
        const std::vector<wng::NodeDefinition> before_copy = before.node_definitions();
        const std::vector<wng::NodeDefinition> after_copy = after.node_definitions();

        const wng::SchemaDiff diff = wng::diff_schemas(before, after);
        assert(diff.result == wng::Result::Ok);
        assert_schema_unchanged(before, before_copy);
        assert_schema_unchanged(after, after_copy);
    }

    {
        // Snapshot diffing must not mutate snapshot DTO contents. Future editor
        // draft comparisons can reuse snapshots after diffing them.
        const wng::GraphSchema schema = schema_with(make_add_definition());
        const wng::SchemaSnapshot before = capture_snapshot(schema);
        const wng::SchemaSnapshot after = capture_snapshot(schema);
        const std::vector<wng::NodeDefinition> before_copy = before.node_definitions;
        const std::vector<wng::NodeDefinition> after_copy = after.node_definitions;

        const wng::SchemaDiff diff = wng::diff_schema_snapshots(before, after);
        assert(diff.result == wng::Result::Ok);
        assert(node_vectors_equal(before.node_definitions, before_copy));
        assert(node_vectors_equal(after.node_definitions, after_copy));
    }

    {
        // Diffing snapshots should not corrupt restored schema behavior. This guards
        // the integration path future schema planning diagnostics will depend on.
        const wng::GraphSchema schema = schema_with(make_add_definition());
        const wng::SchemaSnapshot snapshot = capture_snapshot(schema);
        const wng::SchemaDiff diff = wng::diff_schema_snapshots(snapshot, snapshot);
        assert(diff.result == wng::Result::Ok);

        wng::GraphSchema restored;
        assert(wng::restore_schema_snapshot(restored, snapshot) == wng::Result::Ok);

        wng::Graph graph;
        wng::NodeId node;
        assert(wng::instantiate_node(
            graph,
            restored,
            make_desc("math.add"),
            &node,
            nullptr) == wng::Result::Ok);
        assert(node.value != 0);
        assert(graph.nodes().size() == 1);
        assert(graph.ports().size() == 3);
    }

    return 0;
}
