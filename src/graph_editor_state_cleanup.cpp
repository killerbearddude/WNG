// Implements the editor-state cleanup bridge for graph command/session results.
// Cleanup is intentionally summary-driven: this file does not inspect Graph,
// perform hit testing, execute commands, render UI, or depend on WPL.

#include <wng/graph_editor_state_cleanup.hpp>

namespace wng
{
    void apply_editor_state_cleanup(
        GraphEditorState& editor_state,
        const GraphMutationSummary& summary)
    {
        editor_state.apply_mutation_summary(summary);
    }

    void apply_editor_state_cleanup(
        GraphEditorState& editor_state,
        const GraphCommandRecord& record)
    {
        if (record.result != Result::Ok) {
            return;
        }

        editor_state.apply_mutation_summary(record.summary);
    }

    void apply_editor_state_cleanup(
        GraphEditorState& editor_state,
        const GraphCommandResult& result)
    {
        if (!result.success()) {
            return;
        }

        apply_editor_state_cleanup(editor_state, result.record);
    }

    void apply_editor_state_cleanup(
        GraphEditorState& editor_state,
        const GraphSessionCommandResult& result)
    {
        // A session command can mutate Graph successfully and still fail to record
        // history afterward. Editor state must follow the visible graph mutation,
        // so this bridge keys off command success rather than result.success().
        apply_editor_state_cleanup(editor_state, result.command);
    }

    void apply_editor_state_cleanup(
        GraphEditorState& editor_state,
        const GraphCommandBatch& batch)
    {
        for (const GraphCommandRecord& record : batch.records) {
            apply_editor_state_cleanup(editor_state, record);
        }
    }

    void apply_editor_state_cleanup(
        GraphEditorState& editor_state,
        const GraphCommandTransaction& transaction)
    {
        apply_editor_state_cleanup(editor_state, transaction.batch());
    }
}
