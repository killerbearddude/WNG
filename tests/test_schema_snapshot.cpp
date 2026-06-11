// Exercises in-memory GraphSchema snapshot capture and restore.
// These tests keep schema snapshots scoped to graph-core diagnostics and avoid
// file formats, migrations, WPL integration, or editor state persistence.

#include <cassert>
#include <string>
#include <vector>

#include <wng/execution_plan.hpp>
#include <wng/graph.hpp>
#include <wng/schema_mutation.hpp>
#include <wng/schema_snapshot.hpp>

namespace
{
    wng::PortDefinition input_definition(
        const char* name,
        const char* type = "number",
        bool required = true,
        bool visible = true,
        bool enabled = true)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Input;
        definition.type = type;
        definition.required = required;
        definition.visible = visible;
        definition.enabled = enabled;
        return definition;
    }

    wng::PortDefinition output_definition(
        const char* name,
        const char* type = "number",
        bool required = true,
        bool visible = true,
        bool enabled = true)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Output;
        definition.type = type;
        definition.required = required;
        definition.visible = visible;
        definition.enabled = enabled;
        return definition;
    }

    wng::NodeDefinition make_add_definition()
    {
        wng::NodeDefinition definition;
        definition.type = "math.add";
        definition.display_name = "Add";
        definition.inputs.push_back(input_definition("a"));
        definition.inputs.push_back(input_definition("b", "number", false));
        definition.outputs.push_back(output_definition("result"));
        definition.visible = true;
        definition.enabled = true;
        return definition;
    }

    wng::NodeDefinition make_print_definition()
    {
        wng::NodeDefinition definition;
        definition.type = "debug.print";
        definition.display_name = "Print";
        definition.inputs.push_back(input_definition("value", "string"));
        definition.visible = false;
        definition.enabled = true;
        return definition;
    }

    wng::GraphSchema schema_with(const wng::NodeDefinition& definition)
    {
        wng::GraphSchema schema;
        assert(schema.add_node_definition(definition) == wng::Result::Ok);
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

    bool port_definitions_equal(
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
            port_definitions_equal(a.inputs, b.inputs) &&
            port_definitions_equal(a.outputs, b.outputs);
    }

    bool schema_definitions_equal(
        const wng::GraphSchema& a,
        const wng::GraphSchema& b)
    {
        const std::vector<wng::NodeDefinition>& left = a.node_definitions();
        const std::vector<wng::NodeDefinition>& right = b.node_definitions();

        if (left.size() != right.size()) {
            return false;
        }

        for (std::size_t i = 0; i < left.size(); ++i) {
            if (!node_definition_equal(left[i], right[i])) {
                return false;
            }
        }

        return true;
    }

    wng::NodeDesc make_node_desc(const char* type = "math.add")
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = "Add";
        return desc;
    }
}

int main()
{
    {
        // Verifies empty schemas can be captured as explicit in-memory values.
        // Future editor draft flows need an empty schema to round-trip cleanly.
        const wng::GraphSchema schema;

        const wng::SchemaSnapshotResult result =
            wng::capture_schema_snapshot(schema);

        assert(result.result == wng::Result::Ok);
        assert(result.success());
        assert(result.snapshot.empty());
        assert(result.snapshot.node_definitions.empty());
    }

    {
        // Verifies non-empty snapshots copy node and port definitions by value
        // rather than retaining references into the source schema.
        const wng::GraphSchema schema = schema_with(make_add_definition());

        const wng::SchemaSnapshotResult result =
            wng::capture_schema_snapshot(schema);

        assert(result.result == wng::Result::Ok);
        assert(!result.snapshot.empty());
        assert(result.snapshot.node_definitions.size() == 1U);
        assert(node_definition_equal(
            result.snapshot.node_definitions[0],
            make_add_definition()));
    }

    {
        // Verifies restoring an empty snapshot is replacement, not merge. Existing
        // target definitions must be removed when the snapshot has no definitions.
        wng::GraphSchema empty_schema;
        const wng::SchemaSnapshotResult empty_snapshot =
            wng::capture_schema_snapshot(empty_schema);
        assert(empty_snapshot.result == wng::Result::Ok);

        wng::GraphSchema target = schema_with(make_add_definition());
        assert(wng::restore_schema_snapshot(target, empty_snapshot.snapshot) ==
            wng::Result::Ok);

        assert(target.node_definitions().empty());
    }

    {
        // Verifies restore replaces target schema contents instead of merging. This
        // keeps snapshot semantics simple for future schema draft comparisons.
        const wng::GraphSchema source = schema_with(make_add_definition());
        const wng::SchemaSnapshotResult source_snapshot =
            wng::capture_schema_snapshot(source);
        assert(source_snapshot.result == wng::Result::Ok);

        wng::GraphSchema target = schema_with(make_print_definition());
        assert(wng::restore_schema_snapshot(target, source_snapshot.snapshot) ==
            wng::Result::Ok);

        assert(target.find_node_definition("math.add") != nullptr);
        assert(target.find_node_definition("debug.print") == nullptr);
        assert(schema_definitions_equal(source, target));
    }

    {
        // Verifies all stable schema definition fields survive a snapshot round
        // trip. Schema-aware mutation and planning depend on these fields.
        wng::GraphSchema schema;
        assert(schema.add_node_definition(make_add_definition()) == wng::Result::Ok);
        assert(schema.add_node_definition(make_print_definition()) == wng::Result::Ok);

        const wng::SchemaSnapshotResult snapshot =
            wng::capture_schema_snapshot(schema);
        assert(snapshot.result == wng::Result::Ok);

        wng::GraphSchema restored;
        assert(wng::restore_schema_snapshot(restored, snapshot.snapshot) ==
            wng::Result::Ok);

        assert(schema_definitions_equal(schema, restored));
    }

    {
        // Verifies restored schemas remain operational, not just structurally
        // copied. Instantiation must still create schema-declared ports.
        const wng::GraphSchema source = schema_with(make_add_definition());
        const wng::SchemaSnapshotResult snapshot =
            wng::capture_schema_snapshot(source);
        assert(snapshot.result == wng::Result::Ok);

        wng::GraphSchema restored;
        assert(wng::restore_schema_snapshot(restored, snapshot.snapshot) ==
            wng::Result::Ok);

        wng::Graph graph;
        wng::NodeId node;
        assert(wng::instantiate_node(graph, restored, make_node_desc(), &node, nullptr) ==
            wng::Result::Ok);

        const wng::Node* created = graph.find_node(node);
        assert(created != nullptr);
        assert(created->inputs.size() == 2U);
        assert(created->outputs.size() == 1U);
    }

    {
        // Verifies capture is observational only. Capturing must not reorder or
        // mutate definitions because callers may snapshot during editor previews.
        wng::GraphSchema schema;
        assert(schema.add_node_definition(make_add_definition()) == wng::Result::Ok);
        assert(schema.add_node_definition(make_print_definition()) == wng::Result::Ok);

        const std::vector<wng::NodeDefinition> before = schema.node_definitions();
        const wng::SchemaSnapshotResult snapshot =
            wng::capture_schema_snapshot(schema);

        assert(snapshot.result == wng::Result::Ok);
        assert(schema.node_definitions().size() == before.size());
        for (std::size_t i = 0; i < before.size(); ++i) {
            assert(node_definition_equal(schema.node_definitions()[i], before[i]));
        }
    }

    {
        // Verifies duplicate definitions in a malformed snapshot are rejected and
        // the target schema remains unchanged. Restore atomicity protects callers
        // from partially imported schema drafts.
        wng::SchemaSnapshot invalid_snapshot;
        invalid_snapshot.node_definitions.push_back(make_add_definition());
        invalid_snapshot.node_definitions.push_back(make_add_definition());

        wng::GraphSchema target = schema_with(make_print_definition());
        const wng::GraphSchema before = target;

        const wng::Result result =
            wng::restore_schema_snapshot(target, invalid_snapshot);

        assert(result == wng::Result::AlreadyExists);
        assert(schema_definitions_equal(target, before));
    }

    {
        // Verifies malformed node definitions use the same validation path as
        // normal GraphSchema construction and leave the target schema untouched.
        wng::NodeDefinition invalid = make_add_definition();
        invalid.type.clear();

        wng::SchemaSnapshot invalid_snapshot;
        invalid_snapshot.node_definitions.push_back(invalid);

        wng::GraphSchema target = schema_with(make_print_definition());
        const wng::GraphSchema before = target;

        const wng::Result result =
            wng::restore_schema_snapshot(target, invalid_snapshot);

        assert(result == wng::Result::InvalidArgument);
        assert(schema_definitions_equal(target, before));
    }

    {
        // Verifies restored schema data is sufficient for schema-aware execution
        // planning. Planning regression tests can use snapshots without adding
        // persistence or migration behavior.
        const wng::GraphSchema source = schema_with(make_add_definition());
        const wng::SchemaSnapshotResult snapshot =
            wng::capture_schema_snapshot(source);
        assert(snapshot.result == wng::Result::Ok);

        wng::GraphSchema restored;
        assert(wng::restore_schema_snapshot(restored, snapshot.snapshot) ==
            wng::Result::Ok);

        wng::Graph graph;
        wng::NodeId node;
        assert(wng::instantiate_node(graph, restored, make_node_desc(), &node, nullptr) ==
            wng::Result::Ok);

        const wng::ExecutionPlan plan =
            wng::build_execution_plan(graph, restored, wng::ExecutionPlanRequest {});

        assert(plan.result == wng::Result::Ok);
        assert(plan.success());
    }

    return 0;
}
