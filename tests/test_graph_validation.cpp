// Exercises whole-graph validation reports using only public graph/schema APIs.
// Deep structural corruption checks are implemented in the validator, but tests
// avoid private backdoors and focus on reachable graph states.

#include <cassert>
#include <string>

#include <wng/graph_validation.hpp>
#include <wng/schema_mutation.hpp>

namespace
{
    wng::PortDefinition input(
        const std::string& name,
        const std::string& type = "number",
        bool required = false)
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
        const std::string& type = "number",
        bool required = false)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = wng::PortKind::Output;
        definition.type = type;
        definition.required = required;
        return definition;
    }

    wng::NodeDefinition node_definition(const std::string& type = "math.add")
    {
        wng::NodeDefinition definition;
        definition.type = type;
        definition.display_name = "Add";
        definition.inputs.push_back(input("a", "number", true));
        definition.inputs.push_back(input("b", "number", false));
        definition.outputs.push_back(output("result", "number", true));
        return definition;
    }

    wng::GraphSchema schema_with(const wng::NodeDefinition& definition)
    {
        wng::GraphSchema schema;
        assert(schema.add_node_definition(definition) == wng::Result::Ok);
        return schema;
    }

    wng::NodeDesc desc(const std::string& type = "math.add")
    {
        wng::NodeDesc node;
        node.type = type;
        node.title = "Add";
        return node;
    }

    wng::NodeId create_node(wng::Graph& graph, const std::string& type)
    {
        wng::NodeId node;
        assert(graph.create_node(desc(type), &node) == wng::Result::Ok);
        return node;
    }

    wng::PortId add_port(
        wng::Graph& graph,
        wng::NodeId node,
        wng::PortKind kind,
        const std::string& name,
        const std::string& type)
    {
        wng::PortDesc port;
        port.kind = kind;
        port.name = name;
        port.type = type;

        wng::PortId id;
        assert(graph.add_port(node, port, &id) == wng::Result::Ok);
        return id;
    }

    void connect(wng::Graph& graph, wng::PortId from, wng::PortId to)
    {
        wng::LinkId link;
        assert(graph.create_link(from, to, &link) == wng::Result::Ok);
    }

    wng::GraphValidationOptions require_acyclic()
    {
        wng::GraphValidationOptions options;
        options.cycle_mode = wng::GraphCycleMode::RequireAcyclic;
        return options;
    }

    const wng::ValidationIssue* find_issue(
        const wng::ValidationReport& report,
        wng::ValidationIssueCode code)
    {
        for (const wng::ValidationIssue& issue : report.issues) {
            if (issue.code == code) {
                return &issue;
            }
        }

        return nullptr;
    }

    unsigned count_issues(
        const wng::ValidationReport& report,
        wng::ValidationIssueCode code)
    {
        unsigned count = 0;
        for (const wng::ValidationIssue& issue : report.issues) {
            if (issue.code == code) {
                ++count;
            }
        }

        return count;
    }

    void assert_issue(
        const wng::ValidationReport& report,
        wng::ValidationIssueCode code,
        wng::Result result)
    {
        const wng::ValidationIssue* issue = find_issue(report, code);
        assert(issue != nullptr);
        assert(issue->severity == wng::ValidationSeverity::Error);
        assert(issue->result == result);
    }
}

int main()
{
    {
        // Empty graphs are structurally valid and should produce no diagnostics.
        wng::Graph graph;
        const wng::ValidationReport report = wng::validate_graph(graph);

        assert(report.valid());
        assert(!report.has_errors());
        assert(report.issues.empty());
    }

    {
        // Schema-instantiated nodes contain all declared ports, so both structural
        // and schema validation should pass.
        wng::Graph graph;
        const wng::GraphSchema schema = schema_with(node_definition());

        wng::NodeId node;
        assert(wng::instantiate_node(graph, schema, desc(), &node, nullptr) == wng::Result::Ok);

        assert(wng::validate_graph(graph).valid());
        assert(wng::validate_graph(graph, schema).valid());
    }

    {
        // A graph can be structurally valid while still failing schema validation
        // because its node type is unknown to the schema.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "unknown.node");
        const wng::GraphSchema schema = schema_with(node_definition());

        const wng::ValidationReport report = wng::validate_graph(graph, schema);

        assert(!report.valid());
        assert_issue(report, wng::ValidationIssueCode::MissingNodeDefinition, wng::Result::NotFound);
        assert(find_issue(report, wng::ValidationIssueCode::MissingNodeDefinition)->node == node);
    }

    {
        // Disabled node definitions are invalid for schema use, but the graph
        // structure itself remains unchanged and inspectable.
        wng::Graph graph;
        create_node(graph, "math.add");

        wng::NodeDefinition definition = node_definition();
        definition.enabled = false;
        const wng::GraphSchema schema = schema_with(definition);

        const wng::ValidationReport report = wng::validate_graph(graph, schema);

        assert(!report.valid());
        assert_issue(report, wng::ValidationIssueCode::DisabledNodeDefinition, wng::Result::InvalidConnection);
    }

    {
        // Required input ports are the first active use of PortDefinition::required.
        // Missing required inputs should be reported even when the node exists.
        wng::Graph graph;
        create_node(graph, "math.add");
        const wng::GraphSchema schema = schema_with(node_definition());

        const wng::ValidationReport report = wng::validate_graph(graph, schema);

        assert(!report.valid());
        assert_issue(report, wng::ValidationIssueCode::RequiredPortMissing, wng::Result::InvalidConnection);
    }

    {
        // Required output ports are validated independently from required inputs.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "math.add");
        add_port(graph, node, wng::PortKind::Input, "a", "number");

        const wng::GraphSchema schema = schema_with(node_definition());
        const wng::ValidationReport report = wng::validate_graph(graph, schema);

        assert(!report.valid());
        assert_issue(report, wng::ValidationIssueCode::RequiredPortMissing, wng::Result::InvalidConnection);
    }

    {
        // Undeclared graph ports are schema errors, not structural graph errors.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "math.add");
        add_port(graph, node, wng::PortKind::Input, "extra", "number");

        const wng::GraphSchema schema = schema_with(node_definition());
        const wng::ValidationReport report = wng::validate_graph(graph, schema);

        assert(!report.valid());
        assert_issue(report, wng::ValidationIssueCode::MissingPortDefinition, wng::Result::NotFound);
    }

    {
        // Disabled port definitions are reported even when the matching graph
        // port exists and has otherwise-compatible metadata.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "math.add");
        add_port(graph, node, wng::PortKind::Input, "a", "number");

        wng::NodeDefinition definition = node_definition();
        definition.inputs[0].enabled = false;
        const wng::GraphSchema schema = schema_with(definition);

        const wng::ValidationReport report = wng::validate_graph(graph, schema);

        assert(!report.valid());
        assert_issue(report, wng::ValidationIssueCode::DisabledPortDefinition, wng::Result::InvalidConnection);
    }

    {
        // Graph port types must remain compatible with their schema definitions.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "math.add");
        add_port(graph, node, wng::PortKind::Input, "a", "string");

        const wng::GraphSchema schema = schema_with(node_definition());
        const wng::ValidationReport report = wng::validate_graph(graph, schema);

        assert(!report.valid());
        assert_issue(report, wng::ValidationIssueCode::PortTypeMismatch, wng::Result::InvalidConnection);
    }

    {
        // The existing permissive "any" type semantics should also apply to
        // schema validation.
        wng::Graph graph;
        const wng::NodeId node = create_node(graph, "math.add");
        add_port(graph, node, wng::PortKind::Input, "a", "number");
        add_port(graph, node, wng::PortKind::Output, "result", "number");

        wng::NodeDefinition definition = node_definition();
        definition.inputs[0].type = "any";
        definition.outputs[0].type = "any";
        const wng::GraphSchema schema = schema_with(definition);

        const wng::ValidationReport report = wng::validate_graph(graph, schema);

        assert(find_issue(report, wng::ValidationIssueCode::PortTypeMismatch) == nullptr);
        assert(find_issue(report, wng::ValidationIssueCode::RequiredPortMissing) == nullptr);
    }

    {
        // Bare Graph-created content can still be structurally valid without a
        // schema, which keeps graph-core validation independent from schema policy.
        wng::Graph graph;
        const wng::NodeId source = create_node(graph, "constant.number");
        const wng::NodeId target = create_node(graph, "debug.print");
        const wng::PortId output_port =
            add_port(graph, source, wng::PortKind::Output, "value", "number");
        const wng::PortId input_port =
            add_port(graph, target, wng::PortKind::Input, "value", "number");

        connect(graph, output_port, input_port);

        const wng::ValidationReport report = wng::validate_graph(graph);

        assert(report.valid());
        assert(report.issues.empty());
    }

    {
        // Acyclic mode accepts ordinary source-to-sink graph structure.
        wng::Graph graph;
        const wng::NodeId source = create_node(graph, "constant.number");
        const wng::NodeId target = create_node(graph, "debug.print");
        const wng::PortId output_port =
            add_port(graph, source, wng::PortKind::Output, "value", "number");
        const wng::PortId input_port =
            add_port(graph, target, wng::PortKind::Input, "value", "number");
        connect(graph, output_port, input_port);

        const wng::ValidationReport report = wng::validate_graph(graph, require_acyclic());

        assert(report.valid());
        assert(find_issue(report, wng::ValidationIssueCode::CycleDetected) == nullptr);
    }

    {
        // Default validation remains cycle-neutral. This preserves domain-neutral
        // graph storage while allowing callers to request acyclic diagnostics.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "node.a");
        const wng::NodeId b = create_node(graph, "node.b");
        const wng::PortId a_in = add_port(graph, a, wng::PortKind::Input, "in", "number");
        const wng::PortId a_out = add_port(graph, a, wng::PortKind::Output, "out", "number");
        const wng::PortId b_in = add_port(graph, b, wng::PortKind::Input, "in", "number");
        const wng::PortId b_out = add_port(graph, b, wng::PortKind::Output, "out", "number");
        connect(graph, a_out, b_in);
        connect(graph, b_out, a_in);

        const wng::ValidationReport report = wng::validate_graph(graph);

        assert(report.valid());
        assert(find_issue(report, wng::ValidationIssueCode::CycleDetected) == nullptr);
    }

    {
        // Acyclic mode reports cycle diagnostics in deterministic unresolved-node
        // order provided by topological_sort.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "node.a");
        const wng::NodeId b = create_node(graph, "node.b");
        const wng::PortId a_in = add_port(graph, a, wng::PortKind::Input, "in", "number");
        const wng::PortId a_out = add_port(graph, a, wng::PortKind::Output, "out", "number");
        const wng::PortId b_in = add_port(graph, b, wng::PortKind::Input, "in", "number");
        const wng::PortId b_out = add_port(graph, b, wng::PortKind::Output, "out", "number");
        connect(graph, a_out, b_in);
        connect(graph, b_out, a_in);

        const wng::ValidationReport report = wng::validate_graph(graph, require_acyclic());

        assert(!report.valid());
        assert(count_issues(report, wng::ValidationIssueCode::CycleDetected) == 2U);
        assert(report.issues[0].code == wng::ValidationIssueCode::CycleDetected);
        assert(report.issues[0].node == a);
        assert(report.issues[0].result == wng::Result::InvalidConnection);
        assert(report.issues[1].code == wng::ValidationIssueCode::CycleDetected);
        assert(report.issues[1].node == b);
        assert(report.issues[1].result == wng::Result::InvalidConnection);
    }

    {
        // Schema validation composes with acyclic mode: graph-level cycle issues
        // are reported without suppressing normal schema compatibility checks.
        wng::Graph graph;
        const wng::NodeId a = create_node(graph, "math.add");
        const wng::NodeId b = create_node(graph, "math.add");
        const wng::PortId a_in = add_port(graph, a, wng::PortKind::Input, "a", "number");
        const wng::PortId a_out = add_port(graph, a, wng::PortKind::Output, "result", "number");
        const wng::PortId b_in = add_port(graph, b, wng::PortKind::Input, "a", "number");
        const wng::PortId b_out = add_port(graph, b, wng::PortKind::Output, "result", "number");
        connect(graph, a_out, b_in);
        connect(graph, b_out, a_in);

        const wng::GraphSchema schema = schema_with(node_definition());
        const wng::ValidationReport report =
            wng::validate_graph(graph, schema, require_acyclic());

        assert(!report.valid());
        assert(count_issues(report, wng::ValidationIssueCode::CycleDetected) == 2U);
        assert(find_issue(report, wng::ValidationIssueCode::MissingNodeDefinition) == nullptr);
        assert(find_issue(report, wng::ValidationIssueCode::RequiredPortMissing) == nullptr);
    }

    return 0;
}
