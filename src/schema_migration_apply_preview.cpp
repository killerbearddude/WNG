// Implements read-only schema migration application previews for WNG.
// The preview combines schema-aware policy validation and policy-aware migration
// planning; it classifies readiness without applying any migration changes.

#include <wng/schema_migration_apply_preview.hpp>

#include <new>

#include <wng/graph.hpp>
#include <wng/schema.hpp>

namespace
{
    wng::SchemaMigrationApplyPreview preview_failure(
        wng::Result result,
        wng::SchemaMigrationApplyPreviewStatus status)
    {
        wng::SchemaMigrationApplyPreview preview;
        preview.result = result;
        preview.status = status;
        return preview;
    }

    void classify_actions(
        const wng::SchemaMigrationPlan& plan,
        std::vector<wng::SchemaMigrationAction>& uncovered_blocking,
        std::vector<wng::SchemaMigrationAction>& covered_blocking,
        std::vector<wng::SchemaMigrationAction>& non_blocking)
    {
        // Buckets preserve migration-plan order. The planner already owns
        // deduplication and deterministic action ordering, so preview should not
        // sort, merge, or otherwise reinterpret actions.
        for (const wng::SchemaMigrationAction& action : plan.actions) {
            if (action.blocking && action.policy_covered) {
                covered_blocking.push_back(action);
            } else if (action.blocking) {
                uncovered_blocking.push_back(action);
            } else {
                non_blocking.push_back(action);
            }
        }
    }

    bool has_uncovered_blocking_actions(
        const wng::SchemaMigrationApplyPreview& preview)
    {
        return !preview.uncovered_blocking_actions.empty();
    }

    wng::SchemaMigrationApplyPreviewStatus compute_preview_status(
        const wng::SchemaMigrationPlan& plan,
        bool has_uncovered_blocking)
    {
        // Policy coverage is not migration application. Uncovered blocking
        // actions are reported first; covered blocking actions still leave a
        // target-invalid plan blocked by validation until a future apply layer
        // actually mutates graph data.
        if (has_uncovered_blocking) {
            return wng::SchemaMigrationApplyPreviewStatus::BlockedByUncoveredActions;
        }

        if (!plan.compatible()) {
            return wng::SchemaMigrationApplyPreviewStatus::BlockedByValidation;
        }

        return wng::SchemaMigrationApplyPreviewStatus::Ready;
    }
}

namespace wng
{
    bool SchemaMigrationApplyPreview::success() const
    {
        return result == Result::Ok;
    }

    bool SchemaMigrationApplyPreview::ready() const
    {
        return result == Result::Ok && status == SchemaMigrationApplyPreviewStatus::Ready;
    }

    bool SchemaMigrationApplyPreview::blocked() const
    {
        return !ready();
    }

    SchemaMigrationApplyPreview preview_schema_migration_application(
        const Graph& graph,
        const GraphSchema& source_schema,
        const GraphSchema& target_schema,
        const SchemaMigrationPolicy& policy)
    {
        try {
            SchemaMigrationApplyPreview preview;

            // Schema-aware policy validation runs before planning so incoherent
            // policy entries cannot be treated as usable coverage.
            preview.policy_validation = validate_schema_migration_policy(
                policy,
                source_schema,
                target_schema);
            if (!preview.policy_validation.valid()) {
                preview.result = preview.policy_validation.result;
                preview.status = SchemaMigrationApplyPreviewStatus::PolicyInvalid;
                return preview;
            }

            // The policy-aware planner is the single source for action coverage.
            // Preview is read-only and never calls graph/schema mutation APIs.
            preview.plan = build_schema_migration_plan(
                graph,
                source_schema,
                target_schema,
                policy);
            if (!preview.plan.success()) {
                preview.result = preview.plan.result;
                preview.status = SchemaMigrationApplyPreviewStatus::PlanFailed;
                return preview;
            }

            classify_actions(
                preview.plan,
                preview.uncovered_blocking_actions,
                preview.covered_blocking_actions,
                preview.non_blocking_actions);

            preview.result = Result::Ok;
            preview.status = compute_preview_status(
                preview.plan,
                has_uncovered_blocking_actions(preview));
            return preview;
        } catch (const std::bad_alloc&) {
            return preview_failure(
                Result::AllocationFailure,
                SchemaMigrationApplyPreviewStatus::PlanFailed);
        }
    }
}
