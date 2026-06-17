# WNG Product Roadmap

This roadmap is the active plan for moving WNG from its current graph-core
foundation to a user-facing product. It supersedes the earlier diagnostics-first
working direction.

WNG remains a standalone C++17 graph-engine layer. It must stay deterministic,
testable, extensible, serialization-friendly, and suitable for future node graph
tools without becoming the platform layer, renderer backend, application, game
logic, or domain-specific node library.

## Product objective

Build WNG into an embeddable graph product that lets a consumer:

1. define graph schemas;
2. create and mutate graphs through stable APIs;
3. validate graph state;
4. inspect deterministic traversal and execution-planning metadata;
5. serialize through format-neutral DTO boundaries;
6. migrate graph data across schema versions;
7. undo and redo graph-level operations;
8. host a future graph editor and WPL renderer without leaking UI/platform code
   into `wng_core`.

A user-facing WNG product is not just a collection of primitives. It needs a
cohesive session/document boundary, editor-facing state, graph-space hit testing,
rendering integration, and eventually runtime/evaluation seams.

## Current baseline

The following foundations are considered implemented enough to stop treating
them as the main development track:

- graph-core model: IDs, nodes, ports, links, graph mutation, mutation summaries;
- in-memory DTO import/export and graph snapshots;
- schema definitions and schema-aware mutation helpers;
- whole-graph validation and schema validation;
- deterministic traversal, dependency analysis, dirty propagation, and execution
  planning metadata;
- graph command records, command transactions, command history, and mixed
  graph-level history;
- graph/schema diffing, schema compatibility, schema migration planning,
  migration policy, migration application, and migration history.

These areas still require bug fixes and integration hardening when defects are
found, but they should no longer dominate the roadmap.

## Development pivot

The project is moving out of the diagnostics/test-hardening loop.

Supporting tests and diagnostics remain required for new public behavior, but
they are not the product objective. Future PRs should normally add product
capability first and include only the tests required to prove that capability.

Test-only, diagnostics-only, or one-line cleanup PRs should be avoided unless
one of these is true:

- the issue blocks the next product milestone;
- the issue is a confirmed regression;
- the issue protects a public API boundary that the next milestone will depend
  on;
- the project owner explicitly requests the cleanup.

## Hard boundaries

WNG core must not absorb unrelated layers.

Do not add these to `wng_core`:

- visual node editor widgets;
- canvas rendering implementation;
- WPL headers or WPL include paths;
- platform/windowing code;
- X11, XKB, raw input, or frame lifecycle code;
- GPU or renderer backends;
- game/application-specific node behavior;
- scripting language runtime;
- database, networking, or asset pipeline code;
- file format ownership beyond DTO boundaries until a persistence milestone
  explicitly decides otherwise.

Future editor, runtime, and WPL integration layers may consume `wng_core`, but
`wng_core` must remain domain-neutral.

## Architectural invariants

All future product work must preserve these invariants:

- persistent identity uses stable IDs, not raw pointers;
- public APIs remain explicit and `Result`-based;
- graph mutation summaries remain deterministic;
- validation happens before traversal or planning that depends on validity;
- traversal and planning results are deterministic;
- cycle behavior is explicit and opt-in where required;
- serialization boundaries use DTOs or snapshots, not runtime pointers;
- host extension points are opt-in and do not transfer ownership to graph core;
- core layers do not include editor, renderer, platform, or WPL headers;
- product-facing APIs are cohesive enough that consumers do not need to manually
  stitch together every low-level subsystem.

## Roadmap to user-facing product

### Phase 1: Product-facing graph session

Status: next active milestone.

Goal: add a coherent in-memory boundary that represents one usable graph working
state.

Proposed concept names:

- `GraphSession`; or
- `GraphDocument`.

The name should be chosen before implementation. `GraphSession` is preferred if
it owns runtime working state but not persistence. `GraphDocument` is preferred
if it becomes the editor/persistence-facing model.

Initial scope:

- own or aggregate `Graph`, `GraphSchema`, and `GraphHistory`;
- expose current graph/schema accessors;
- track revision and dirty state;
- provide session-level validation;
- provide session-level execution planning;
- provide command recording, undo, and redo routing;
- provide graph snapshot/export/import handoff points;
- remain in-memory and format-neutral;
- avoid editor UI, WPL, rendering, and runtime evaluation.

This phase turns existing primitives into a product-shaped API.

### Phase 2: Editor model core without WPL

Status: planned after graph session.

Goal: define graph-editor state without platform or rendering dependencies.

Scope:

- selected nodes, ports, and links;
- hovered graph item;
- active interaction mode as data, not UI behavior;
- pending link creation state;
- editor cleanup after graph mutation summaries;
- editor-safe command grouping through graph/session history.

Non-goals:

- no widgets;
- no WPL dependency;
- no screen-to-canvas conversion;
- no rendering;
- no OS input handling.

### Phase 3: Graph-space hit testing

Status: planned after editor model core.

Goal: let editor/integration layers identify graph elements from canonical
graph-space coordinates.

Scope:

- node body hit testing;
- port hit testing;
- link hit testing using graph-space geometry;
- deterministic hit priority;
- configurable hit radii and geometry policy;
- no screen/canvas transform ownership.

WPL remains responsible for screen-to-canvas and canvas-to-screen conversion.

### Phase 4: Static WPL rendering adapter

Status: planned after graph-space editor state and hit testing.

Goal: render a static graph through WPL draw commands without moving rendering
ownership into `wng_core`.

Likely target:

- `wng_render_wpl` depends on `wng_core`, the editor/model layer if needed, and
  WPL;
- render nodes, ports, labels, links, selection, hover, and validation overlays;
- use straight-line link fallback until WPL exposes richer primitives;
- keep all WPL headers out of `wng_core`.

This phase creates the first visual user-facing product surface.

### Phase 5: Interactive graph editing

Status: planned after static rendering.

Goal: support real user operations through editor/session APIs.

Scope:

- select and deselect graph objects;
- move selected nodes in graph space;
- start, update, cancel, and complete link creation;
- delete selected graph objects;
- group user operations into undoable commands;
- integrate mutation summaries with editor-state cleanup.

Non-goals:

- no application-specific menus or inspectors;
- no custom node libraries;
- no platform input ownership inside WNG core.

### Phase 6: Product example application

Status: planned after interactive editing.

Goal: provide a minimal application that proves WNG can be embedded as a
user-facing graph tool.

Scope:

- create a window through WPL;
- render a graph;
- select and drag nodes;
- create links;
- validate graph state;
- undo and redo graph operations;
- save/load only if a persistence boundary has been explicitly chosen.

This example is not the engine core. It is the proof that the engine core and
integration layers are usable.

### Phase 7: Runtime/evaluation boundary

Status: deferred until the editor/product shell is usable.

Goal: decide whether WNG should execute graphs or only prepare deterministic
execution metadata.

Possible scope:

- evaluator registration interface;
- typed value boundary or opaque host value handles;
- execution context lifetime rules;
- plan execution using existing `ExecutionPlan` metadata;
- deterministic failure reporting;
- no threaded/asynchronous scheduler until explicitly designed.

This phase must not introduce domain-specific node behavior into graph core.

### Phase 8: Persistence/file-format decision

Status: deferred.

Goal: decide whether WNG owns a file format or remains DTO-only.

Options:

1. WNG remains DTO-only and applications own JSON/binary formats.
2. WNG provides a reference text/binary format outside `wng_core`.
3. WNG provides adapters but leaves storage policy to the host.

Until this decision is made, file I/O remains out of core.

## Near-term PR queue

The next development PRs should be product-progress PRs, in this order unless a
blocking defect appears:

1. `GraphSession` or `GraphDocument` foundation.
2. Session-level command/history integration.
3. Session-level validation and execution-planning convenience APIs.
4. Editor model core: selection, hover, pending link state.
5. Mutation-summary driven editor-state cleanup.
6. Graph-space hit testing.
7. Static WPL renderer adapter design and first implementation.
8. Interactive selection and node dragging.
9. Interactive link creation.
10. Minimal product example application.

## PR sizing policy

Prefer medium, coherent product slices over micro-PRs.

A good product PR should generally:

- add or complete one user-visible capability;
- touch the subsystem files needed for that capability;
- include supporting tests only where they prove the new behavior;
- update docs when the public workflow changes;
- avoid mixing unrelated product milestones.

Avoid PRs that only rename, polish, or harden internals unless they unblock the
current product milestone.

## Definition of user-facing product

WNG reaches its first user-facing product target when a consumer can:

1. define a schema;
2. create a graph through the session/editor-facing API;
3. render the graph through WPL integration;
4. select, move, connect, and delete graph objects;
5. validate graph state and surface validation feedback;
6. undo and redo user graph operations;
7. import/export through an agreed boundary;
8. extend node/schema behavior without modifying graph-core internals.

This target does not require game logic, a scripting runtime, a full asset
pipeline, or a finished visual design system.

## Current best next direction

The best immediate development direction is `GraphSession` / `GraphDocument`.

The next code PR should establish the product-facing in-memory working-state
boundary that future editor, WPL rendering, and runtime layers will consume.
Validation, diagnostics, and tests should support that work rather than replace
it.
