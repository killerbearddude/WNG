// Provides whole-graph validation reports for WNG.
// This layer inspects graph structure and optional schema consistency without
// mutating Graph or GraphSchema.

#pragma once

#include <string>
#include <vector>

#include <wng/ids.hpp>
#include <wng/result.hpp>
#include <wng/schema_validation.hpp>

namespace wng
{
    class Graph;
    class GraphSchema;

    enum class ValidationSeverity {
        Error,
        Warning
    };

    enum class ValidationIssueCode {
        InvalidNodeId,
        InvalidPortId,
        InvalidLinkId,
        MissingNode,
        MissingPort,
        DuplicateNodeId,
        DuplicatePortId,
        DuplicateLinkId,
        InvalidPortKind,
        InvalidNodeGeometry,
        PortOwnedByMissingNode,
        NodeReferencesMissingPort,
        NodeReferencesForeignPort,
        NodeReferencesWrongPortDirection,
        LinkReferencesMissingPort,
        LinkDirectionMismatch,
        LinkSameNode,
        LinkDuplicate,
        LinkMultipleIntoInput,
        LinkTypeMismatch,
        MissingNodeDefinition,
        MissingPortDefinition,
        DisabledNodeDefinition,
        DisabledPortDefinition,
        RequiredPortMissing,
        PortTypeMismatch,
        SchemaConnectionRejected,
        CycleDetected,
        HostValidationIssue,
        ResourceExhausted
    };

    enum class GraphCycleMode {
        AllowCycles,
        RequireAcyclic
    };

    struct ValidationIssue {
        ValidationSeverity severity = ValidationSeverity::Error;
        ValidationIssueCode code = ValidationIssueCode::InvalidNodeId;
        Result result = Result::InvalidArgument;

        NodeId node;
        PortId port;
        LinkId link;

        std::string message;
    };

    struct ValidationReport {
        std::vector<ValidationIssue> issues;

        bool valid() const;
        bool has_errors() const;
    };

    class GraphValidationCallback {
    public:
        virtual ~GraphValidationCallback() = default;

        virtual Result validate_graph(
            const Graph& graph,
            ValidationReport& report) const = 0;
    };

    struct GraphValidationOptions {
        GraphCycleMode cycle_mode = GraphCycleMode::AllowCycles;
        const GraphValidationCallback* callback = nullptr;
    };

    struct GraphSchemaValidationOptions {
        explicit GraphSchemaValidationOptions(
            const GraphValidationOptions& graph_options,
            const SchemaValidationOptions& schema_options = SchemaValidationOptions {});

        explicit GraphSchemaValidationOptions(
            const SchemaValidationOptions& schema_options);

        GraphValidationOptions graph_options;
        SchemaValidationOptions schema_options;
    };

    ValidationReport validate_graph(const Graph& graph);

    ValidationReport validate_graph(
        const Graph& graph,
        const GraphValidationOptions& options);

    ValidationReport validate_graph(const Graph& graph, const GraphSchema& schema);

    ValidationReport validate_graph(
        const Graph& graph,
        const GraphSchema& schema,
        const GraphValidationOptions& options);

    ValidationReport validate_graph(
        const Graph& graph,
        const GraphSchema& schema,
        const GraphSchemaValidationOptions& options);
}
