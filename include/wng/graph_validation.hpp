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

    // Optional host extension point for domain-specific graph validation.
    // Implementations must treat Graph as read-only and append diagnostics only to
    // the supplied report. The graph core does not own callback lifetime.
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

    // Combines graph-level validation options with schema connection validation
    // options so whole-graph schema validation can reuse proposed-connection host
    // schema policy without merging the two callback lifetimes or responsibilities.
    struct GraphSchemaValidationOptions {
        explicit GraphSchemaValidationOptions(
            const GraphValidationOptions& graph_options,
            const SchemaValidationOptions& schema_options = SchemaValidationOptions {});

        explicit GraphSchemaValidationOptions(
            const SchemaValidationOptions& schema_options);

        GraphValidationOptions graph_options;
        SchemaValidationOptions schema_options;
    };

    // Performs non-mutating structural validation using only current Graph state.
    // This overload does not require or consult a GraphSchema and allows cycles.
    ValidationReport validate_graph(const Graph& graph);

    // Performs structural validation with explicit graph-level validation options.
    // Acyclic checking and host callbacks are opt-in so graph storage remains
    // domain-neutral by default.
    ValidationReport validate_graph(
        const Graph& graph,
        const GraphValidationOptions& options);

    // Performs structural validation first, then appends schema-consistency issues.
    // Schema validation extends structural validation and never hides graph issues.
    ValidationReport validate_graph(const Graph& graph, const GraphSchema& schema);

    // Performs structural validation with explicit graph-level validation options,
    // then appends schema-consistency issues and host diagnostics without hiding
    // earlier graph issues.
    ValidationReport validate_graph(
        const Graph& graph,
        const GraphSchema& schema,
        const GraphValidationOptions& options);

    // Performs structural and schema validation with explicit graph and schema
    // validation options. Schema connection callbacks are applied to existing
    // links only after structural and built-in schema checks have succeeded.
    ValidationReport validate_graph(
        const Graph& graph,
        const GraphSchema& schema,
        const GraphSchemaValidationOptions& options);
}
