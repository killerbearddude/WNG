// Previews whether a schema migration is ready to be applied.
// This layer combines schema-aware policy validation with policy-aware planning,
// but it never mutates graphs, schemas, or policies.

#pragma once

#include <vector>

#include <wng/result.hpp>
#include <wng/schema_migration_plan.hpp>
#include <wng/schema_migration_policy.hpp>

namespace wng
{
    class Graph;
    class GraphSchema;

    // High-level readiness outcome for a read-only migration apply preview.
    // Non-ready statuses describe why a future apply layer should not proceed.
    enum class SchemaMigrationApplyPreviewStatus {
        Ready,
        PolicyInvalid,
        PlanFailed,
        BlockedByUncoveredActions,
        BlockedByValidation
    };

    // Read-only application preview. It preserves the policy validation result,
    // the full migration plan, and stable action buckets for diagnostics.
    struct SchemaMigrationApplyPreview {
        Result result = Result::Ok;
        SchemaMigrationApplyPreviewStatus status =
            SchemaMigrationApplyPreviewStatus::Ready;

        SchemaMigrationPolicySchemaValidation policy_validation;
        SchemaMigrationPlan plan;

        std::vector<SchemaMigrationAction> uncovered_blocking_actions;
        std::vector<SchemaMigrationAction> covered_blocking_actions;
        std::vector<SchemaMigrationAction> non_blocking_actions;

        bool success() const;
        bool ready() const;
        bool blocked() const;
    };

    // Builds a read-only preview for applying a schema migration. A preview is
    // ready only when policy validation succeeds, planning succeeds, target
    // validation is compatible, and no blocking actions remain uncovered.
    SchemaMigrationApplyPreview preview_schema_migration_application(
        const Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema,
        const SchemaMigrationPolicy& policy);
}
