// Bridges graph command/session mutation summaries into WNG editor state cleanup.
// This module keeps GraphSession focused on graph/schema/history while allowing
// editor-facing state to discard stable IDs removed by graph mutations.

#pragma once

#include <wng/graph_command.hpp>
#include <wng/graph_command_transaction.hpp>
#include <wng/graph_editor_state.hpp>
#include <wng/graph_session.hpp>

namespace wng
{
    // Applies one already-built mutation summary to editor state. This is a thin
    // named bridge for callers that want cleanup semantics without reaching into
    // GraphEditorState directly.
    void apply_editor_state_cleanup(
        GraphEditorState& editor_state,
        const GraphMutationSummary& summary);

    // Applies cleanup for one command record. Failed records are ignored because
    // they did not produce reliable graph mutation effects.
    void apply_editor_state_cleanup(
        GraphEditorState& editor_state,
        const GraphCommandRecord& record);

    // Applies cleanup for one command result. Cleanup follows command success,
    // independent of any later history-recording step.
    void apply_editor_state_cleanup(
        GraphEditorState& editor_state,
        const GraphCommandResult& result);

    // Applies cleanup for one session-routed command result. If graph mutation
    // succeeded but session history recording failed, cleanup still runs because
    // the visible graph state already changed.
    void apply_editor_state_cleanup(
        GraphEditorState& editor_state,
        const GraphSessionCommandResult& result);

    // Applies cleanup for successful records in one logical command batch.
    void apply_editor_state_cleanup(
        GraphEditorState& editor_state,
        const GraphCommandBatch& batch);

    // Applies cleanup for successful records currently held by a transaction.
    // The transaction must contain command results produced against the same graph
    // whose editor state is being cleaned.
    void apply_editor_state_cleanup(
        GraphEditorState& editor_state,
        const GraphCommandTransaction& transaction);
}
