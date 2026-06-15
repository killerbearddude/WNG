// Exercises whole-graph validation reports using only public graph/schema APIs.
// Deep structural corruption checks are implemented in the validator, but tests
// avoid private backdoors and focus on reachable graph states.

#include <cassert>
#include <new>
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

    wng::NodeDesc empty_title_desc(const std::string& type = "math.add")
    {
        wng::NodeDesc node;
        node.type = type;
        return node;
    }

    wng::NodeId create_node(wng::Graph& graph, const std::string& type)
    {
        wng::NodeId node;
        assert(graph.create_node(desc(type), &node) == wng::Result::Ok);
        return node;
    }

    wng::NodeId create_node_with_empty_title(wng::Graph& graph, const std::string& type)
    {
        wng::NodeId node;
        assert(graph.create_node(empty_title_desc(type), &node) == wng::Result::Ok);
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

    wng::LinkId connect(wng::Graph& graph, wng::PortId from, wng::PortId to)
    {
        wng::LinkId link;
        assert(graph.create_link(from, to, &link) == wng::Result::Ok);
        return link;
    }

    wng::GraphValidationOptions require_acyclic()
    {
        wng::GraphValidationOptions options;
        options.cycle_mode = wng::GraphCycleMode::RequireAcyclic;
        return options;
    }

    struct SchemaLinkedGraph {
        wng::Graph graph;
        wng::GraphSchema schema;
        wng::PortId from;
        wng::PortId to;
        wng::LinkId link;
    };

    SchemaLinkedGraph make_schema_linked_graph()
    {
        SchemaLinkedGraph fixture;
        fixture.schema = schema_with(node_definition());

        const wng::NodeId source = create_node(fixture.graph, "math.add");
        const wng::NodeId target = create_node(fixture.graph, "math.add");
        add_port(fixture.graph, source, wng::PortKind::Input, "a", "number");
        fixture.from = add_port(fixture.graph, source, wng::PortKind::Output, "result", "number");
        fixture.to = add_port(fixture.graph, target, wng::PortKind::Input, "a", "number");
        add_port(fixture.graph, target, wng::PortKind::Output, "result", "number");
        fixture.link = connect(fixture.graph, fixture.from, fixture.to);

        return fixture;
    }

    class EmptyTitleCallback final : public wng::GraphValidationCallback {
    public:
        mutable unsigned calls = 0;

        wng::Result validate_graph(
            const wng::Graph& graph,
            wng::ValidationReport& report) const override
        {
            ++calls;

            // Host diagnostics are domain-specific and are appended after core
            // validation. This test callback treats an empty title as a host error.
            for (const wng::Node& node : graph.nodes()) {
                if (!node.title.empty()) {
                    continue;
                }

                wng::ValidationIssue issue;
                issue.severity = wng::ValidationSeverity::Error;
                issue.code = wng::ValidationIssueCode::HostValidationIssue;
                issue.result = wng::Result::InvalidArgument;
                issue.node = node.id;
                issue.message = "host requires node title";
                report.issues.push_back(issue);
            }

            return wng::Result::Ok;
        }
    };

    class FailingCallback final : public wng::GraphValidationCallback {
    public:
        wng::Result validate_graph(
            const wng::Graph&,
            wng::ValidationReport&) const override
        {
            return wng::Result::InvalidArgument;
        }
    };

    class CountingSchemaConnectionCallback final : public wng::SchemaValidationCallback {
    public:
        mutable unsigned calls = 0;
        mutable wng::PortId last_from;
        mutable wng::PortId last_to;

        wng::ConnectionValidation validate_connection(
            const wng::Graph& graph,
            const wng::GraphSchema&,
            wng::PortId from,
            wng::PortId to) const override
        {
            ++calls;
            last_from = from;
            last_to = to;
            assert(graph.find_port(from) != nullptr);
            assert(graph.find_port(to) != nullptr);

            wng::ConnectionValidation validation;
            validation.status = wng::ConnectionStatus::Allowed;
            validation.result = wng::Result::Ok;
            return validation;
        }
    };

    class RejectingSchemaConnectionCallback final : public wng::SchemaValidationCallback {
    public:
        mutable unsigned calls = 0;

        wng::ConnectionValidation validate_connection(
            const wng::Graph&,
            const wng::GraphSchema&,
            wng::PortId,
            wng::PortId) const override
        {
            ++calls;

            wng::ConnectionValidation validation;
            validation.status = wng::ConnectionStatus::Rejected;
            validation.result = wng::Result::InvalidConnection;
            return validation;
        }
    };

    class AllocatingSchemaConnectionCallback final : public wng::SchemaValidationCallback {
    public:
        mutable unsigned calls = 0;

        wng::ConnectionValidation validate_connection(
            const wng::Graph&,
            const wng::GraphSchema&,
            wng::PortId,
            wng::PortId) const override
        {
            ++calls;
            throw std::bad_alloc();
        }
    };

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

    {
        // Without a callback, host-specific diagnostics are absent and the default
        // validation path remains unchanged.
        wng::Graph graph;
        create_node_with_empty_title(graph, "host.node");

        const wng::ValidationReport report = wng::validate_graph(graph);

        assert(report.valid());
        assert(find_issue(report, wng::ValidationIssueCode::HostValidationIssue) == nullptr);
    }

    {
        // Host callbacks append diagnostics after built-in structural validation.
        wng::Graph graph;
        const wng::NodeId node = create_node_with_empty_title(graph, "host.node");
        EmptyTitleCallback callback;
        wng::GraphValidationOptions options;
        options.callback = &callback;

        const wng::ValidationReport report = wng::validate_graph(graph, options);

        assert(callback.calls == 1U);
        assert(!report.valid());
        assert(report.issues.size() == 1U);
        assert(report.issues[0].code == wng::ValidationIssueCode::HostValidationIssue);
        assert(report.issues[0].node == node);
        assert(report.issues[0].result == wng::Result::InvalidArgument);
    }

    {
        // Schema validation diagnostics are emitted before host diagnostics, giving
        // callers deterministic layering from core to schema to host checks.
        wng::Graph graph;
        const wng::NodeId node = create_node_with_empty_title(graph, "unknown.node");
        const wng::GraphSchema schema = schema_with(node_definition());
        EmptyTitleCallback callback;
        wng::GraphValidationOptions options;
        options.callback = &callback;

        const wng::ValidationReport report = wng::validate_graph(graph, schema, options);

        assert(callback.calls == 1U);
        assert(!report.valid());
        assert(report.issues.size() == 2U);
        assert(report.issues[0].code == wng::ValidationIssueCode::MissingNodeDefinition);
        assert(report.issues[0].node == node);
        assert(report.issues[1].code == wng::ValidationIssueCode::HostValidationIssue);
        assert(report.issues[1].node == node);
    }

    {
        // Schema connection callbacks are applied to existing links only after
        // structural and built-in schema validation have succeeded.
        SchemaLinkedGraph fixture = make_schema_linked_graph();
        CountingSchemaConnectionCallback callback;
        wng::GraphSchemaValidationOptions options;
        options.schema_options.callback = &callback;

        const wng::ValidationReport report =
            wng::validate_graph(fixture.graph, fixture.schema, options);

        assert(callback.calls == 1U);
        assert(callback.last_from == fixture.from);
        assert(callback.last_to == fixture.to);
        assert(report.valid());
        assert(find_issue(report, wng::ValidationIssueCode::SchemaConnectionRejected) == nullptr);
    }

    {
        // A rejecting schema connection callback becomes a deterministic link issue
        // without mutating the graph or weakening built-in schema validation.
        SchemaLinkedGraph fixture = make_schema_linked_graph();
        RejectingSchemaConnectionCallback callback;
        wng::GraphSchemaValidationOptions options;
        options.schema_options.callback = &callback;

        const wng::ValidationReport report =
            wng::validate_graph(fixture.graph, fixture.schema, options);

        assert(callback.calls == 1U);
        assert(!report.valid());
        assert(report.issues.size() == 1U);
        assert(report.issues[0].code == wng::ValidationIssueCode::SchemaConnectionRejected);
        assert(report.issues[0].link == fixture.link);
        assert(report.issues[0].result == wng::Result::InvalidConnection);
    }

    {
        // Schema connection callbacks are skipped when earlier schema validation
        // already found errors, so host policy does not obscure core diagnostics.
        wng::Graph graph;
        const wng::NodeId source = create_node(graph, "unknown.source");
        const wng::NodeId target = create_node(graph, "unknown.target");
        const wng::PortId from = add_port(graph, source, wng::PortKind::Output, "value", "number");
        const wng::PortId to = add_port(graph, target, wng::PortKind::Input, "value", "number");
        connect(graph, from, to);

        const wng::GraphSchema schema = schema_with(node_definition());
        CountingSchemaConnectionCallback callback;
        wng::GraphSchemaValidationOptions options;
        options.schema_options.callback = &callback;

        const wng::ValidationReport report = wng::validate_graph(graph, schema, options);

        assert(callback.calls == 0U);
        assert(!report.valid());
        assert(count_issues(report, wng::ValidationIssueCode::MissingNodeDefinition) == 2U);
        assert(find_issue(report, wng::ValidationIssueCode::SchemaConnectionRejected) == nullptr);
    }

    {
        // Allocation failure from schema connection callbacks maps into the normal
        // validation report instead of escaping the public API.
        SchemaLinkedGraph fixture = make_schema_linked_graph();
        AllocatingSchemaConnectionCallback callback;
        wng::GraphSchemaValidationOptions options;
        options.schema_options.callback = &callback;

        const wng::ValidationReport report =
            wng::validate_graph(fixture.graph, fixture.schema, options);

        assert(callback.calls == 1U);
        assert(!report.valid());
        assert(report.issues.size() == 1U);
        assert(report.issues[0].code == wng::ValidationIssueCode::SchemaConnectionRejected);
        assert(report.issues[0].link == fixture.link);
        assert(report.issues[0].result == wng::Result::AllocationFailure);
    }

    {
        // Graph host callbacks still run after schema connection callback issues,
        // preserving deterministic layering through schema-specific and graph-host checks.
        SchemaLinkedGraph fixture = make_schema_linked_graph();
        RejectingSchemaConnectionCallback schema_callback;
        EmptyTitleCallback graph_callback;
        wng::GraphSchemaValidationOptions options;
        options.schema_options.callback = &schema_callback;
        options.graph_options.callback = &graph_callback;

        const wng::ValidationReport report =
            wng::validate_graph(fixture.graph, fixture.schema, options);

        assert(schema_callback.calls == 1U);
        assert(graph_callback.calls == 1U);
        assert(!report.valid());
        assert(report.issues.size() == 1U);
        assert(report.issues[0].code == wng::ValidationIssueCode::SchemaConnectionRejected);
    }

    {
        // A non-Ok callback result is converted into a host validation issue even
        // when the callback did not append its own diagnostic.
        wng::Graph graph;
        FailingCallback callback;
        wng::GraphValidationOptions options;
        options.callback = &callback;

        const wng::ValidationReport report = wng::validate_graph(graph, options);

        assert(!report.valid());
        assert(report.issues.size() == 1U);
        assert(report.issues[0].code == wng::ValidationIssueCode::HostValidationIssue);
        assert(report.issues[0].result == wng::Result::InvalidArgument);
    }

    {
        // The host callback receives a const Graph. Validation must not alter graph
        // storage counts or IDs while host diagnostics are appended.
        wng::Graph graph;
        const wng::NodeId node = create_node_with_empty_title(graph, "host.node");
        const std::size_t node_count = graph.nodes().size();
        const std::size_t port_count = graph.ports().size();
        const std::size_t link_count = graph.links().size();
        EmptyTitleCallback callback;
        wng::GraphValidationOptions options;
        options.callback = &callback;

        const wng::ValidationReport report = wng::validate_graph(graph, options);

        assert(!report.valid());
        assert(graph.find_node(node) != nullptr);
        assert(graph.nodes().size() == node_count);
        assert(graph.ports().size() == port_count);
        assert(graph.links().size() == link_count);
    }

    return 0;
}
