// Exercises validation diagnostics for resource exhaustion paths.
// The graph validator must report resource exhaustion explicitly when the
// diagnostic is graph-global, while preserving link anchors for link-local schema
// callback failures.

#include <cassert>
#include <new>

#include <wng/graph.hpp>
#include <wng/graph_validation.hpp>
#include <wng/schema.hpp>

namespace
{
    wng::Result resource_exhausted_result()
    {
        return static_cast<wng::Result>(5);
    }

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

    struct LinkedFixture {
        wng::Graph graph;
        wng::GraphSchema schema;
        wng::LinkId link;
    };

    LinkedFixture make_linked_fixture()
    {
        LinkedFixture fixture;
        fixture.schema = make_schema();

        const wng::NodeId source = add_node(fixture.graph);
        const wng::NodeId target = add_node(fixture.graph);
        const wng::PortId from = add_port(fixture.graph, source, wng::PortKind::Output, "out");
        const wng::PortId to = add_port(fixture.graph, target, wng::PortKind::Input, "in");

        assert(fixture.graph.create_link(from, to, &fixture.link) == wng::Result::Ok);
        return fixture;
    }

    class ThrowingValidationCallback final : public wng::GraphValidationCallback {
    public:
        mutable unsigned calls = 0;

        wng::Result validate_graph(
            const wng::Graph&,
            wng::ValidationReport&) const override
        {
            ++calls;
            throw std::bad_alloc();
        }
    };

    class ThrowingSchemaConnectionCallback final : public wng::SchemaValidationCallback {
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
}

int main()
{
    {
        // Public graph validation catches host resource exhaustion and returns a
        // deterministic diagnostic that is not tied to a fake node or link error.
        wng::Graph graph;
        ThrowingValidationCallback callback;
        wng::GraphValidationOptions options;
        options.callback = &callback;

        const wng::ValidationReport report = wng::validate_graph(graph, options);

        assert(callback.calls == 1U);
        assert(!report.valid());
        assert(report.issues.size() == 1U);
        assert(report.issues[0].code == wng::ValidationIssueCode::ResourceExhausted);
        assert(report.issues[0].result == resource_exhausted_result());
        assert(report.issues[0].node == wng::NodeId {});
        assert(report.issues[0].port == wng::PortId {});
        assert(report.issues[0].link == wng::LinkId {});
    }

    {
        // Host resource exhaustion is appended after earlier graph diagnostics
        // instead of replacing them. This preserves deterministic issue layering
        // for callers that need both core diagnostics and callback failure state.
        wng::Graph graph;
        const wng::NodeId source = add_node(graph);
        const wng::NodeId target = add_node(graph);
        const wng::PortId source_in = add_port(graph, source, wng::PortKind::Input, "in");
        const wng::PortId source_out = add_port(graph, source, wng::PortKind::Output, "out");
        const wng::PortId target_in = add_port(graph, target, wng::PortKind::Input, "in");
        const wng::PortId target_out = add_port(graph, target, wng::PortKind::Output, "out");

        wng::LinkId link;
        assert(graph.create_link(source_out, target_in, &link) == wng::Result::Ok);
        assert(graph.create_link(target_out, source_in, &link) == wng::Result::Ok);

        ThrowingValidationCallback callback;
        wng::GraphValidationOptions options;
        options.cycle_mode = wng::GraphCycleMode::RequireAcyclic;
        options.callback = &callback;

        const wng::ValidationReport report = wng::validate_graph(graph, options);

        assert(callback.calls == 1U);
        assert(!report.valid());
        assert(report.issues.size() == 3U);
        assert(report.issues[0].code == wng::ValidationIssueCode::CycleDetected);
        assert(report.issues[0].node == source);
        assert(report.issues[1].code == wng::ValidationIssueCode::CycleDetected);
        assert(report.issues[1].node == target);
        assert(report.issues[2].code == wng::ValidationIssueCode::ResourceExhausted);
        assert(report.issues[2].result == resource_exhausted_result());
        assert(report.issues[2].node == wng::NodeId {});
        assert(report.issues[2].port == wng::PortId {});
        assert(report.issues[2].link == wng::LinkId {});
    }

    {
        // Schema connection callback resource exhaustion is link-local. The
        // diagnostic keeps the schema-connection issue code and preserves the
        // failed link anchor while still reporting the resource-exhaustion result.
        LinkedFixture fixture = make_linked_fixture();
        ThrowingSchemaConnectionCallback callback;
        wng::SchemaValidationOptions schema_options;
        schema_options.callback = &callback;
        const wng::GraphSchemaValidationOptions options(schema_options);

        const wng::ValidationReport report =
            wng::validate_graph(fixture.graph, fixture.schema, options);

        assert(callback.calls == 1U);
        assert(!report.valid());
        assert(report.issues.size() == 1U);
        assert(report.issues[0].code == wng::ValidationIssueCode::SchemaConnectionRejected);
        assert(report.issues[0].result == resource_exhausted_result());
        assert(report.issues[0].node == wng::NodeId {});
        assert(report.issues[0].port == wng::PortId {});
        assert(report.issues[0].link == fixture.link);
    }

    return 0;
}
