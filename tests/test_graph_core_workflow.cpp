// Exercises a cohesive WNG graph-core workflow through the public aggregate API.
// The test intentionally crosses schema-backed mutation, validation, execution
// planning, DTO export/import, and diffing without introducing UI, rendering, or
// domain-specific runtime evaluation behavior.

#include <cassert>
#include <string>
#include <vector>

#include <wng/wng.hpp>

namespace
{
    struct WorkflowNode {
        wng::NodeId id;
        std::vector<wng::PortId> inputs;
        std::vector<wng::PortId> outputs;
    };

    struct WorkflowGraph {
        wng::GraphSchema schema;
        wng::Graph graph;
        WorkflowNode source_a;
        WorkflowNode source_b;
        WorkflowNode multiply;
        WorkflowNode sink;
    };

    wng::PortDefinition input_definition(const char* name)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Input;
        definition.type = "number";
        definition.required = true;
        return definition;
    }

    wng::PortDefinition output_definition(const char* name)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Output;
        definition.type = "number";
        definition.required = true;
        return definition;
    }

    wng::NodeDefinition constant_definition()
    {
        wng::NodeDefinition definition;
        definition.type = "constant.number";
        definition.display_name = "Number Constant";
        definition.outputs.push_back(output_definition("value"));
        return definition;
    }

    wng::NodeDefinition multiply_definition()
    {
        wng::NodeDefinition definition;
        definition.type = "math.multiply";
        definition.display_name = "Multiply";
        definition.inputs.push_back(input_definition("left"));
        definition.inputs.push_back(input_definition("right"));
        definition.outputs.push_back(output_definition("result"));
        return definition;
    }

    wng::NodeDefinition sink_definition()
    {
        wng::NodeDefinition definition;
        definition.type = "debug.sink";
        definition.display_name = "Debug Sink";
        definition.inputs.push_back(input_definition("value"));
        return definition;
    }

    void add_workflow_schema_definitions(wng::GraphSchema& schema)
    {
        // The workflow uses schema-aware mutation for every node and link, so
        // missing or incompatible definitions should fail before any plan is built.
        assert(schema.add_node_definition(constant_definition()) == wng::Result::Ok);
        assert(schema.add_node_definition(multiply_definition()) == wng::Result::Ok);
        assert(schema.add_node_definition(sink_definition()) == wng::Result::Ok);
    }

    wng::NodeDesc node_desc(const char* type, const char* title, float x)
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = title;
        desc.position = wng::Vec2 { x, 40.0f };
        desc.size = wng::Vec2 { 120.0f, 64.0f };
        return desc;
    }

    WorkflowNode instantiate_workflow_node(
        wng::Graph& graph,
        const wng::GraphSchema& schema,
        const char* type,
        const char* title,
        float x)
    {
        WorkflowNode result;
        assert(wng::instantiate_node(
            graph,
            schema,
            node_desc(type, title, x),
            &result.id,
            nullptr) == wng::Result::Ok);

        const wng::Node* node = graph.find_node(result.id);
        assert(node != nullptr);
        result.inputs = node->inputs;
        result.outputs = node->outputs;
        return result;
    }

    void connect(
        wng::Graph& graph,
        const wng::GraphSchema& schema,
        wng::PortId from,
        wng::PortId to)
    {
        wng::LinkId link;
        assert(wng::create_link(graph, schema, from, to, &link) == wng::Result::Ok);
    }

    WorkflowGraph build_schema_backed_workflow()
    {
        WorkflowGraph workflow;
        add_workflow_schema_definitions(workflow.schema);

        workflow.source_a = instantiate_workflow_node(
            workflow.graph,
            workflow.schema,
            "constant.number",
            "A",
            0.0f);
        workflow.source_b = instantiate_workflow_node(
            workflow.graph,
            workflow.schema,
            "constant.number",
            "B",
            160.0f);
        workflow.multiply = instantiate_workflow_node(
            workflow.graph,
            workflow.schema,
            "math.multiply",
            "Multiply",
            320.0f);
        workflow.sink = instantiate_workflow_node(
            workflow.graph,
            workflow.schema,
            "debug.sink",
            "Sink",
            480.0f);

        assert(workflow.source_a.outputs.size() == 1U);
        assert(workflow.source_b.outputs.size() == 1U);
        assert(workflow.multiply.inputs.size() == 2U);
        assert(workflow.multiply.outputs.size() == 1U);
        assert(workflow.sink.inputs.size() == 1U);

        connect(
            workflow.graph,
            workflow.schema,
            workflow.source_a.outputs[0],
            workflow.multiply.inputs[0]);
        connect(
            workflow.graph,
            workflow.schema,
            workflow.source_b.outputs[0],
            workflow.multiply.inputs[1]);
        connect(
            workflow.graph,
            workflow.schema,
            workflow.multiply.outputs[0],
            workflow.sink.inputs[0]);

        return workflow;
    }

    void assert_nodes(
        const std::vector<wng::NodeId>& actual,
        const std::vector<wng::NodeId>& expected)
    {
        assert(actual.size() == expected.size());
        for (std::vector<wng::NodeId>::size_type i = 0; i < expected.size(); ++i) {
            assert(actual[i] == expected[i]);
        }
    }

    const wng::ExecutionPlanStep& step_for(
        const wng::ExecutionPlan& plan,
        wng::NodeId node)
    {
        for (const wng::ExecutionPlanStep& step : plan.steps) {
            if (step.node == node) {
                return step;
            }
        }

        assert(false);
        return plan.steps[0];
    }

    void assert_valid_schema_graph(
        const wng::Graph& graph,
        const wng::GraphSchema& schema)
    {
        const wng::ValidationReport report = wng::validate_graph(graph, schema);
        assert(report.valid());
        assert(!report.has_errors());
    }

    void assert_whole_graph_plan(
        const wng::Graph& graph,
        const wng::GraphSchema& schema,
        const WorkflowGraph& ids)
    {
        const wng::ExecutionPlan plan = wng::build_execution_plan(
            graph,
            schema,
            wng::ExecutionPlanRequest {});

        assert(plan.result == wng::Result::Ok);
        assert(plan.success());
        assert(plan.complete());
        assert(plan.unresolved_nodes.empty());
        assert_nodes(plan.source_nodes, std::vector<wng::NodeId> {
            ids.source_a.id,
            ids.source_b.id
        });
        assert_nodes(plan.planned_nodes, std::vector<wng::NodeId> {
            ids.source_a.id,
            ids.source_b.id,
            ids.multiply.id,
            ids.sink.id
        });

        // Dependencies follow link storage order, which is part of WNG's
        // deterministic planning contract for multi-input nodes.
        assert_nodes(step_for(plan, ids.multiply.id).dependencies, std::vector<wng::NodeId> {
            ids.source_a.id,
            ids.source_b.id
        });
        assert_nodes(step_for(plan, ids.sink.id).dependencies, std::vector<wng::NodeId> {
            ids.multiply.id
        });
    }

    void assert_dto_equal(const wng::GraphDto& a, const wng::GraphDto& b)
    {
        assert(a.version.major == b.version.major);
        assert(a.version.minor == b.version.minor);
        assert(a.version.patch == b.version.patch);
        assert(a.nodes.size() == b.nodes.size());
        assert(a.ports.size() == b.ports.size());
        assert(a.links.size() == b.links.size());

        for (std::vector<wng::NodeDto>::size_type i = 0; i < a.nodes.size(); ++i) {
            assert(a.nodes[i].id == b.nodes[i].id);
            assert(a.nodes[i].type == b.nodes[i].type);
            assert(a.nodes[i].title == b.nodes[i].title);
            assert(a.nodes[i].position.x == b.nodes[i].position.x);
            assert(a.nodes[i].position.y == b.nodes[i].position.y);
            assert(a.nodes[i].size.x == b.nodes[i].size.x);
            assert(a.nodes[i].size.y == b.nodes[i].size.y);
            assert(a.nodes[i].inputs == b.nodes[i].inputs);
            assert(a.nodes[i].outputs == b.nodes[i].outputs);
            assert(a.nodes[i].visible == b.nodes[i].visible);
            assert(a.nodes[i].enabled == b.nodes[i].enabled);
        }

        for (std::vector<wng::PortDto>::size_type i = 0; i < a.ports.size(); ++i) {
            assert(a.ports[i].id == b.ports[i].id);
            assert(a.ports[i].node == b.ports[i].node);
            assert(a.ports[i].kind == b.ports[i].kind);
            assert(a.ports[i].name == b.ports[i].name);
            assert(a.ports[i].type == b.ports[i].type);
            assert(a.ports[i].visible == b.ports[i].visible);
            assert(a.ports[i].enabled == b.ports[i].enabled);
        }

        for (std::vector<wng::LinkDto>::size_type i = 0; i < a.links.size(); ++i) {
            assert(a.links[i].id == b.links[i].id);
            assert(a.links[i].from == b.links[i].from);
            assert(a.links[i].to == b.links[i].to);
            assert(a.links[i].visible == b.links[i].visible);
            assert(a.links[i].enabled == b.links[i].enabled);
        }
    }
}

int main()
{
    // This is an end-to-end graph-core workflow, not a UI or evaluator test:
    // schema-backed creation produces a valid graph, validation approves it,
    // planning derives deterministic dependency order, serialization round-trips
    // stable IDs, and diffing confirms no structural drift after import.
    WorkflowGraph workflow = build_schema_backed_workflow();

    assert_valid_schema_graph(workflow.graph, workflow.schema);
    assert_whole_graph_plan(workflow.graph, workflow.schema, workflow);

    wng::GraphDto exported;
    assert(wng::export_graph(workflow.graph, &exported) == wng::Result::Ok);

    wng::Graph imported;
    assert(wng::import_graph(exported, &imported) == wng::Result::Ok);

    assert_valid_schema_graph(imported, workflow.schema);
    assert_whole_graph_plan(imported, workflow.schema, workflow);

    wng::GraphDto exported_again;
    assert(wng::export_graph(imported, &exported_again) == wng::Result::Ok);
    assert_dto_equal(exported, exported_again);

    const wng::GraphDiff diff = wng::diff_graphs(workflow.graph, imported);
    assert(diff.success());
    assert(diff.empty());

    return 0;
}
