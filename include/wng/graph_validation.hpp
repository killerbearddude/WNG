// Provides whole-graph validation reports for WNG.
// This layer inspects graph structure and optional schema consistency without
// mutating Graph or GraphSchema.

#pragma once

#include <string>
#include <vector>

#include <wng/ids.hpp>
#include <wng/result.hpp>

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
        PortTypeMismatch
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

    // Performs non-mutating structural validation using only current Graph state.
    // This overload does not require or consult a GraphSchema.
    ValidationReport validate_graph(const Graph& graph);

    // Performs structural validation first, then appends schema-consistency issues.
    // Schema validation extends structural validation and never hides graph issues.
    ValidationReport validate_graph(const Graph& graph, const GraphSchema& schema);
}
