// Exercises whole-graph schema validation callback diagnostics.
// These tests focus on deterministic issue ordering and callback result
// normalization without mutating Graph or GraphSchema state.

#include <cassert>
#include <vector>

#include <wng/graph.hpp>
#include <wng/graph_validation.hpp>
#include <wng/schema.hpp>

namespace
{
    wng::PortDefinition port_definition(
        const char* name,
        wng::PortKind kind)
    {
        wng::PortDefinition definition;
        definition.name = name;
        definition.kind = kind;
        definition.type = "number";
        return definition;
    }

    wng::NodeDefinition node_definition()
    {
        wng::NodeDefinition definition;
        definition.type = "test.node";
        definition.display_name = "Test Node";
        definition.inputs.push_back(port_definition("in", wng::PortKind::Input));
        definition.outputs.push_back(port_definition("out", wng::PortKind::Output));
        return definition;
    }

    wng::GraphSchema make_schema()
    {
        wng::GraphSchema schema;
        assert(schema.add_node_definition(node_definition()) == wng::Result::Ok);
        return schema;
    }

    wng::NodeId add_node(wng::Graph& graph)
    {
        wng::NodeDesc desc;
        desc.type = "test.node";
        desc.title = "Test Node";

        wng::NodeId node;
        assert(graph.create_node(desc, &node) == wng::Result::Ok);
        return node;
    }

    wng::PortId add_port(
        wng::Graph& graph,
        wng::NodeId node,
        wng::PortKind kind,
        const char* name)
    {
        wng::PortDesc desc;
        desc.kind = kind;
        desc.name = name;
        desc.type = "number";

        wng::PortId port;
        assert(graph.add_port(node, desc, &port) == wng::Result::Ok);
        return port;
    }

    wng::LinkId connect(wng::Graph& graph, wng::PortId from, wng::PortId to)
    {
        wng::LinkId link;
        assert(graph.create_link(from, to, &link) == wng::Result::Ok);
        return link;
    }

    struct TwoLinkFixture {
        wng::Graph graph;
        wng::GraphSchema schema;
        wng::PortId first_from;
        wng::PortId first_to;
        wng::PortId second_from;
        wng::PortId second_to;
        wng::LinkId first_link;
        wng::LinkId second_link;
    };

    TwoLinkFixture make_two_link_fixture()
    {
        TwoLinkFixture fixture;
        fixture.schema = make_schema();

        const wng::NodeId first_source = add_node(fixture.graph);
        const wng::NodeId first_target = add_node(fixture.graph);
        const wng::NodeId second_source = add_node(fixture.graph);
        const wng::NodeId second_target = add_node(fixture.graph);

        fixture.first_from = add_port(fixture.graph, first_source, wng::PortKind::Output, "out");
        fixture.first_to = add_port(fixture.graph, first_target, wng::PortKind::Input, "in");
        fixture.second_from = add_port(fixture.graph, second_source, wng::PortKind::Output, "out");
        fixture.second_to = add_port(fixture.graph, second_target, wng::PortKind::Input, "in");

        fixture.first_link = connect(fixture.graph, fixture.first_from, fixture.first_to);
        fixture.second_link = connect(fixture.graph, fixture.second_from, fixture.second_to);

        return fixture;
    }

    wng::GraphSchemaValidationOptions options_with(
        const wng::SchemaValidationCallback& callback)
    {
        wng::SchemaValidationOptions schema_options;
        schema_options.callback = &callback;
        return wng::GraphSchemaValidationOptions(schema_options);
    }

    class RecordingRejectingCallback final : public wng::SchemaValidationCallback {
    public:
        mutable std::vector<wng::PortId> from_ports;
        mutable std::vector<wng::PortId> to_ports;

        wng::ConnectionValidation validate_connection(
            const wng::Graph&,
            const wng::GraphSchema&,
            wng::PortId from,
            wng::PortId to) const override
        {
            from_ports.push_back(from);
            to_ports.push_back(to);

            wng::ConnectionValidation validation;
            validation.status = wng::ConnectionStatus::Rejected;
            validation.result = wng::Result::InvalidConnection;
            return validation;
        }
    };

    class RejectingOkCallback final : public wng::SchemaValidationCallback {
    public:
        wng::ConnectionValidation validate_connection(
            const wng::Graph&,
            const wng::GraphSchema&,
            wng::PortId,
            wng::PortId) const override
        {
            // Rejected with Ok is malformed host output. Graph validation should
            // normalize it to InvalidConnection so diagnostics remain actionable.
            wng::ConnectionValidation validation;
            validation.status = wng::ConnectionStatus::Rejected;
            validation.result = wng::Result::Ok;
            return validation;
        }
    };

    class AllowingNonOkCallback final : public wng::SchemaValidationCallback {
    public:
        wng::ConnectionValidation validate_connection(
            const wng::Graph&,
            const wng::GraphSchema&,
            wng::PortId,
            wng::PortId) const override
        {
            // Allowed with a non-Ok result is also malformed host output. The
            // explicit result should be preserved so host failures are visible.
            wng::ConnectionValidation validation;
            validation.status = wng::ConnectionStatus::Allowed;
            validation.result = wng::Result::InvalidArgument;
            return validation;
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
}

int main()
{
    {
        // Existing-link schema callback diagnostics must follow Graph::links()
        // order so editor diagnostics and regression tests remain reproducible.
        TwoLinkFixture fixture = make_two_link_fixture();
        RecordingRejectingCallback callback;

        const wng::ValidationReport report =
            wng::validate_graph(fixture.graph, fixture.schema, options_with(callback));

        assert(!report.valid());
        assert(callback.from_ports.size() == 2U);
        assert(callback.to_ports.size() == 2U);
        assert(callback.from_ports[0] == fixture.first_from);
        assert(callback.to_ports[0] == fixture.first_to);
        assert(callback.from_ports[1] == fixture.second_from);
        assert(callback.to_ports[1] == fixture.second_to);
        assert(report.issues.size() == 2U);
        assert(report.issues[0].code == wng::ValidationIssueCode::SchemaConnectionRejected);
        assert(report.issues[0].link == fixture.first_link);
        assert(report.issues[0].result == wng::Result::InvalidConnection);
        assert(report.issues[1].code == wng::ValidationIssueCode::SchemaConnectionRejected);
        assert(report.issues[1].link == fixture.second_link);
        assert(report.issues[1].result == wng::Result::InvalidConnection);
    }

    {
        // Rejected callbacks that accidentally return Ok are normalized to a
        // deterministic InvalidConnection issue.
        TwoLinkFixture fixture = make_two_link_fixture();
        RejectingOkCallback callback;

        const wng::ValidationReport report =
            wng::validate_graph(fixture.graph, fixture.schema, options_with(callback));
        const wng::ValidationIssue* issue =
            find_issue(report, wng::ValidationIssueCode::SchemaConnectionRejected);

        assert(!report.valid());
        assert(issue != nullptr);
        assert(issue->link == fixture.first_link);
        assert(issue->result == wng::Result::InvalidConnection);
    }

    {
        // Allowed callbacks with a non-Ok result still produce diagnostics because
        // host code reported a failure result.
        TwoLinkFixture fixture = make_two_link_fixture();
        AllowingNonOkCallback callback;

        const wng::ValidationReport report =
            wng::validate_graph(fixture.graph, fixture.schema, options_with(callback));
        const wng::ValidationIssue* issue =
            find_issue(report, wng::ValidationIssueCode::SchemaConnectionRejected);

        assert(!report.valid());
        assert(issue != nullptr);
        assert(issue->link == fixture.first_link);
        assert(issue->result == wng::Result::InvalidArgument);
    }

    return 0;
}
