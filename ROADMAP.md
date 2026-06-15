# WNG Graph Core Roadmap

This roadmap exists to prevent architectural drift.

WNG is a standalone graph-engine core. It should remain deterministic, testable,
extensible, serialization-friendly, and suitable for future node graph tools
without becoming the editor, renderer, runtime, platform layer, or application.

This document is not a date schedule. It is a phase gate and scope filter for
future patches.

## Primary objective

Build and maintain a clean C++ graph core for node graphs, dataflow graphs,
dependency graphs, editor tooling foundations, visual scripting foundations,
procedural systems, and future canvas-based graph tools.

The core owns graph model behavior, validation, traversal, dependency analysis,
execution planning metadata, serialization boundaries, command records,
transaction/history integration points, and extension mechanisms.

The core does not own domain behavior, editor presentation, rendering, platform
integration, runtime evaluation, or application-specific node libraries.

## Hard non-goals

Do not add these to WNG graph core unless this roadmap is deliberately revised:

- visual node editor UI;
- canvas rendering;
- GUI widgets;
- layout engine;
- hit testing and selection UI;
- WPL integration code;
- platform/windowing code;
- GPU or renderer abstractions;
- engine-specific bindings;
- game logic;
- application-specific node behavior;
- scripting language runtime;
- threaded/asynchronous scheduler;
- file format ownership beyond DTO boundaries;
- database, networking, asset pipeline, or package management.

A future editor, runtime, or WPL integration may consume WNG, but WNG must not
absorb those layers.

## Architectural invariants

All future patches should preserve these invariants:

- persistent identity uses stable IDs, not raw pointers;
- public APIs remain explicit and Result-based;
- graph mutation summaries remain deterministic;
- validation happens before traversal or planning that depends on validity;
- traversal and planning results are deterministic;
- cycle behavior is explicit and opt-in where required;
- serialization boundaries use DTOs or snapshots, not runtime pointers;
- host extension points are opt-in and do not weaken core safety;
- callbacks do not transfer ownership to graph core;
- core layers do not include editor, renderer, platform, or WPL headers;
- tests cover every new public API and every new graph invariant.

## Phase gates

### Phase 0: Graph-core baseline

Status: implemented.

The baseline includes IDs, graph-owned nodes, ports, links, built-in connection
validation, deterministic mutation summaries, and C++17 test coverage.

Allowed work in this phase now is limited to bug fixes, invariant hardening, and
small documentation corrections.

### Phase 1: Serialization boundary

Status: implemented for in-memory DTO import/export.

The graph core may define DTOs, snapshots, versioned in-memory structures, and
migration-ready data boundaries. It must not claim ownership of JSON, binary,
asset files, or WPL file I/O until a separate persistence boundary is designed.

### Phase 2: Schema foundation

Status: implemented.

Schema work may include node definitions, port definitions, schema-aware helpers,
connection validation, graph validation, and host schema validation callbacks.

Schema work must remain domain-neutral. New node libraries or gameplay/tool
semantics belong outside the graph core.

### Phase 3: Validation and diagnostics

Status: active hardening.

Validation is the preferred next area for narrow, high-value patches. Good work
includes stronger whole-graph consistency checks, option plumbing, deterministic
issue ordering, callback integration, and allocation-failure handling.

Validation patches must not mutate graphs or schemas.

### Phase 4: Traversal, dependency analysis, and planning

Status: implemented for deterministic non-executing analysis.

Traversal, topological ordering, dirty propagation, and execution planning may
produce ordered metadata. They must not evaluate nodes, store runtime values, or
own scheduler state.

Stored dirty flags and runtime execution state are deferred until ownership and
lifetime rules are designed.

### Phase 5: Command, transaction, and history foundations

Status: implemented for command records, undo/redo primitives, command history,
transactions, schema migration command history, and mixed graph-level history.

Future work may harden transaction boundaries and command-record consistency.

Editor transaction managers, selection history, editor state history, and
automatic command recording remain outside the graph core until an integration
boundary is explicitly designed.

### Phase 6: Diff, snapshot, compatibility, and migration foundations

Status: implemented for deterministic graph/schema diffs, graph/schema snapshots,
schema compatibility, migration planning, migration policy, migration apply
preview, migration command preview, migration apply, and migration history.

Future work should prefer read-only diagnostics and deterministic policy plumbing
before adding mutation-generation semantics.

Graph command generation for schema migration is deferred until there is a clear
decision whether migrations should become ordinary graph command records,
snapshot-backed graph-level records, or a separate command family.

### Phase 7: Runtime execution boundary

Status: deferred.

Evaluator callbacks, runtime node evaluation, stored execution state, and
threaded/asynchronous scheduling are not graph-core priorities yet.

Execution planning may continue to prepare deterministic metadata, but actual
runtime behavior should wait for a dedicated execution boundary design.

### Phase 8: Editor and WPL integration boundary

Status: deferred.

WNG may expose APIs that are useful to editors, but must not add editor UI,
selection systems, WPL dependencies, layout, hit testing, rendering, or platform
code directly to graph core.

Any future WPL/editor integration should live in a separate integration layer.

## Preferred near-term patches

Prefer patches in this order when choosing the next viable change:

1. validation consistency and callback plumbing;
2. deterministic diagnostics and issue ordering;
3. allocation-failure hardening for public Result-based APIs;
4. small command/history invariant improvements;
5. snapshot/diff/migration diagnostic hardening;
6. documentation that prevents scope drift.

Avoid jumping to runtime execution, editor state, WPL integration, file formats,
or domain-specific node behavior while graph-core seams are still being hardened.

## Patch acceptance rules

A patch is a good fit when it satisfies most of these conditions:

- it has one clear architectural purpose;
- it is graph-core only;
- it preserves existing public behavior unless explicitly changing it;
- it has deterministic ordering rules;
- it has tests for success, failure, and boundary cases;
- it catches allocation failures where existing public APIs already promise
  Result-style failure reporting;
- it improves extension seams without giving host code ownership of core state;
- it does not require editor, renderer, WPL, platform, or domain-specific code.

A patch should be rejected or split when it:

- combines model, UI, runtime, and persistence concerns;
- introduces global mutable registries without clear ownership;
- uses raw pointers as persistent identity;
- weakens validation or permits schemas/callbacks to bypass core safety;
- makes traversal or planning nondeterministic;
- stores runtime-only data in serialization DTOs;
- adds editor or WPL dependencies to graph core;
- implements domain-specific node behavior before the core boundary is stable.

## Current best next direction

The best immediate development direction is validation consistency.

The next code patch should prefer threading schema validation callback behavior
into whole-graph schema validation, so proposed-connection host schema policies
and existing-graph validation can share the same opt-in rule path.

That work should stay read-only, deterministic, and graph-core only.
