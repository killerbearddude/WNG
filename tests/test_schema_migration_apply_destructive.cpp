// Exercises destructive schema migration application.
// Tests verify policy-gated removals, dependent link cleanup, stable-ID behavior,
// graph diff reporting, and atomicity for DTO-backed destructive migration.

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include <wng/graph.hpp>
#include <wng/graph_diff.hpp>
#include <wng/graph_validation.hpp>
#include <wng/schema_migration_apply.hpp>
#include <wng/schema_mutation.hpp>

namespace
{
    wng::PortDefinition input(
        const std::string& name,
        const std::string& type = "number",
        bool required = true)
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

    wng::NodeDefinition node_definition(
        const std::string& type,
        bool with_ports = true)
    {
        wng::NodeDefinition definition;
        definition.type = type;
        definition.display_name = type;
        if (with_ports) {
            definition.inputs.push_back(input("value", "number", true));
            definition.outputs.push_back(output("result", "number", false));
        }
        return definition;
    }

    wng::GraphSchema make_schema(const std::vector<wng::NodeDefinition>& definitions)
    {
        wng::GraphSchema schema;
        for (const wng::NodeDefinition& definition : definitions) {
            assert(schema.add_node_definition(definition) == wng::Result::Ok);
        }
        return schema;
    }

    wng::NodeDesc node_desc(const std::string& type)
    {
        wng::NodeDesc desc;
        desc.type = type;
        desc.title = type;
        desc.size = { 100.0f, 60.0f };
        return desc;
    }

    wng::NodeId instantiate_node(
        wng::Graph& graph,
        const wng::GraphSchema& schema,
        const std::string& type)
    {
        wng::NodeId node;
        assert(wng::instantiate_node(graph, schema, node_desc(type), &node, nullptr) ==
            wng::Result::Ok);
        return node;
    }

    const wng::Port* find_port(
        const wng::Graph& graph,
        wng::NodeId node,
        wng::PortKind kind,
        const std::string& name)
    {
        for (const wng::Port& port : graph.ports()) {
            if (port.node == node && port.kind == kind && port.name == name) {
                return &port;
            }
        }
        return nullptr;
    }

    wng::LinkId connect_ports(wng::Graph& graph, wng::PortId from, wng::PortId to)
    {
        wng::LinkId link;
        assert(graph.create_link(from, to, &link) == wng::Result::Ok);
        return link;
    }

    wng::PortDefinitionIdentity port_identity(
        const std::string& node_type,
        wng::PortKind kind,
        const std::string& name)
    {
        wng::PortDefinitionIdentity identity;
        identity.node_type = node_type;
        identity.kind = kind;
        identity.name = name;
        return identity;
    }

    void assert_graph_unchanged(const wng::Graph& before, const wng::Graph& after)
    {
        const wng::GraphDiff diff = wng::diff_graphs(before, after);
        assert(diff.result == wng::Result::Ok);
        assert(diff.empty());
    }

    bool graph_contains_port_id(const wng::Graph& graph, wng::PortId id)
    {
        return graph.find_port(id) != nullptr;
    }

    bool graph_contains_link_id(const wng::Graph& graph, wng::LinkId id)
    {
        return graph.find_link(id) != nullptr;
    }

    bool diff_has_removed_port(const wng::GraphDiff& diff, wng::PortId id)
    {
        for (const wng::PortDiff& port : diff.ports) {
            if (port.change == wng::GraphDiffChange::Removed && port.id == id) {
                return true;
            }
        }
        return false;
    }

    bool diff_has_removed_link(const wng::GraphDiff& diff, wng::LinkId id)
    {
        for (const wng::LinkDiff& link : diff.links) {
            if (link.change == wng::GraphDiffChange::Removed && link.id == id) {
                return true;
            }
        }
        return false;
    }

    std::uint32_t max_port_id(const wng::Graph& graph)
    {
        std::uint32_t max_id = 0U;
        for (const wng::Port& port : graph.ports()) {
            if (port.id.value > max_id) {
                max_id = port.id.value;
            }
        }
        return max_id;
    }
}

int main()
{
    {
        // Destructive node migration removes dependent links before removing ports.
        // Target validation depends on no dangling link endpoint references.
        const wng::GraphSchema source = make_schema({
            node_definition("producer"),
            node_definition("obsolete"),
            node_definition("consumer") });
        const wng::GraphSchema target = make_schema({
            node_definition("producer"),
            node_definition("consumer") });
        wng::Graph graph;
        const wng::NodeId producer = instantiate_node(graph, source, "producer");
        const wng::NodeId obsolete = instantiate_node(graph, source, "obsolete");
        const wng::NodeId consumer = instantiate_node(graph, source, "consumer");
        const wng::LinkId into_obsolete = connect_ports(
            graph,
            find_port(graph, producer, wng::PortKind::Output, "result")->id,
            find_port(graph, obsolete, wng::PortKind::Input, "value")->id);
        const wng::LinkId out_of_obsolete = connect_ports(
            graph,
            find_port(graph, obsolete, wng::PortKind::Output, "result")->id,
            find_port(graph, consumer, wng::PortKind::Input, "value")->id);
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_node_removals.push_back({ "obsolete" });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::Applied);
        assert(graph.find_node(producer) != nullptr);
        assert(graph.find_node(consumer) != nullptr);
        assert(graph.find_node(obsolete) == nullptr);
        assert(!graph_contains_link_id(graph, into_obsolete));
        assert(!graph_contains_link_id(graph, out_of_obsolete));
        assert(diff_has_removed_link(result.diff, into_obsolete));
        assert(diff_has_removed_link(result.diff, out_of_obsolete));
        assert(wng::validate_graph(graph, target).valid());
    }

    {
        // Port removal keeps unrelated ports and links. Only links touching the
        // removed port are cleaned up during destructive DTO migration.
        wng::NodeDefinition source_math = node_definition("math.add");
        source_math.inputs.push_back(input("remove_me", "number", false));
        const wng::GraphSchema source = make_schema({ node_definition("producer"), source_math });
        const wng::GraphSchema target = make_schema({
            node_definition("producer"),
            node_definition("math.add") });
        wng::Graph graph;
        const wng::NodeId producer = instantiate_node(graph, source, "producer");
        const wng::NodeId math = instantiate_node(graph, source, "math.add");
        const wng::PortId value = find_port(graph, math, wng::PortKind::Input, "value")->id;
        const wng::PortId removed = find_port(graph, math, wng::PortKind::Input, "remove_me")->id;
        const wng::LinkId kept_link = connect_ports(
            graph,
            find_port(graph, producer, wng::PortKind::Output, "result")->id,
            value);
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_port_removals.push_back({
            port_identity("math.add", wng::PortKind::Input, "remove_me") });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::Applied);
        assert(graph_contains_port_id(graph, value));
        assert(!graph_contains_port_id(graph, removed));
        assert(graph_contains_link_id(graph, kept_link));
        assert(diff_has_removed_port(result.diff, removed));
        assert(wng::validate_graph(graph, target).valid());
    }

    {
        // Mixed migrations apply in preview order: non-destructive metadata updates
        // and required port creation happen before policy-covered destructive cleanup.
        const wng::GraphSchema source = make_schema({
            node_definition("old.utility", false),
            node_definition("math.add"),
            node_definition("obsolete") });
        wng::NodeDefinition utility_target = node_definition("utility", false);
        wng::NodeDefinition math_target = node_definition("math.add");
        math_target.inputs[0] = input("a", "number", false);
        math_target.inputs.push_back(input("extra", "number", true));
        const wng::GraphSchema target = make_schema({ utility_target, math_target });
        wng::Graph graph;
        const wng::NodeId utility = instantiate_node(graph, source, "old.utility");
        const wng::NodeId math = instantiate_node(graph, source, "math.add");
        const wng::NodeId obsolete = instantiate_node(graph, source, "obsolete");
        const std::uint32_t max_before = max_port_id(graph);
        wng::SchemaMigrationPolicy policy;
        policy.node_type_renames.push_back({ "old.utility", "utility" });
        policy.port_renames.push_back({
            port_identity("math.add", wng::PortKind::Input, "value"),
            port_identity("math.add", wng::PortKind::Input, "a") });
        policy.required_port_defaults.push_back({
            port_identity("math.add", wng::PortKind::Input, "extra"),
            "0" });
        policy.acknowledged_node_removals.push_back({ "obsolete" });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::Applied);
        assert(result.applied_steps.size() == 4U);
        assert(graph.find_node(utility)->type == "utility");
        assert(find_port(graph, math, wng::PortKind::Input, "a") != nullptr);
        const wng::Port* extra = find_port(graph, math, wng::PortKind::Input, "extra");
        assert(extra != nullptr);
        assert(extra->id.value > max_before);
        assert(graph.find_node(obsolete) == nullptr);
        assert(wng::validate_graph(graph, target).valid());
    }

    {
        // Preview readiness can reject an uncovered target incompatibility before
        // destructive DTO rewrites begin. This still verifies the original graph is
        // preserved when a destructive migration request cannot safely proceed.
        const wng::GraphSchema source = make_schema({
            node_definition("obsolete"),
            node_definition("survivor", false) });
        wng::NodeDefinition survivor_target = node_definition("survivor", false);
        survivor_target.enabled = false;
        const wng::GraphSchema target = make_schema({ survivor_target });
        wng::Graph graph;
        instantiate_node(graph, source, "obsolete");
        instantiate_node(graph, source, "survivor");
        const wng::Graph before = graph;
        wng::SchemaMigrationPolicy policy;
        policy.acknowledged_node_removals.push_back({ "obsolete" });

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::NotReady);
        assert(result.result == wng::Result::InvalidArgument);
        assert_graph_unchanged(before, graph);
    }

    {
        // An uncovered destructive schema change is still blocked before apply can
        // guess removal behavior. Explicit policy remains required for data loss.
        const wng::GraphSchema source = make_schema({ node_definition("obsolete") });
        const wng::GraphSchema target;
        wng::Graph graph;
        instantiate_node(graph, source, "obsolete");
        const wng::Graph before = graph;
        const wng::SchemaMigrationPolicy policy;

        const wng::SchemaMigrationApplyResult result =
            wng::apply_schema_migration(graph, source, target, policy);

        assert(result.status == wng::SchemaMigrationApplyStatus::NotReady);
        assert(result.result == wng::Result::InvalidArgument);
        assert_graph_unchanged(before, graph);
    }

    return 0;
}
