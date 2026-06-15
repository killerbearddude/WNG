# WNG

Whacky Node-Graph Layer is a standalone C++17 node graph engine core.

WNG-0.1 implements only the graph-core milestone:

- strong `NodeId`, `PortId`, and `LinkId` types
- graph-space `Vec2` and `Rect` data types
- `Result`-based public APIs
- `Node`, `Port`, `Link`, `NodeDesc`, and `PortDesc`
- `Graph` storage and mutation APIs
- built-in connection validation
- deterministic graph mutation summaries
- C++17 unit tests

WNG-0.1 intentionally does not include WPL integration, rendering, editor state,
selection, hit testing, replay, platform code, or X11.

## WNG-0.2 status

WNG-0.2 begins serialization support with format-agnostic in-memory DTO types.

Current WNG-0.2 status:

- DTO type declarations: implemented
- graph export: implemented
- graph import: implemented
- file I/O: intentionally out of scope

Import/export operate only on in-memory DTOs and do not define a file format.

## Schema status

WNG includes a schema foundation for node and port definitions.

Current schema status:

- node definition registration: implemented
- port definition storage: implemented
- schema-aware connection validation: implemented
- schema-aware node creation helper: implemented
- schema-aware port creation helper: implemented
- schema-aware link creation helper: implemented
- schema-defined node instantiation: implemented
- host validation callbacks: not yet implemented

## Node type identity

Graph nodes carry a stable `type` string distinct from display title.

The `type` field is intended to identify the schema/domain node definition,
while `title` remains display-facing text.

Schema-aware graph mutation is implemented through opt-in free helpers.

`instantiate_node` creates a node and its schema-declared ports. It does not auto-fill display titles from schema definitions and does not create links.

## Validation status

WNG includes built-in connection validation and schema-aware connection validation.

Current validation status:

- built-in connection validation: implemented
- schema-aware connection validation: implemented
- whole-graph structural validation: implemented
- whole-graph schema validation: implemented
- acyclic graph validation option: implemented
- host validation callbacks: not yet implemented

## Traversal status

WNG includes deterministic, non-mutating traversal helpers.

Current traversal status:

- downstream reachability: implemented
- upstream reachability: implemented
- deterministic topological sort for acyclic graphs: implemented
- cycle reporting for topological sort: implemented
- acyclic graph validation option: implemented
- execution planning: implemented

## Dependency analysis status

WNG includes deterministic traversal and dirty propagation helpers.

Current dependency analysis status:

- upstream reachability: implemented
- downstream reachability: implemented
- deterministic topological sort: implemented
- dirty propagation from changed nodes: implemented
- dirty propagation from changed ports: implemented
- dirty propagation from changed links: implemented
- stored dirty flags: not implemented
- execution planning: implemented

## Execution planning status

WNG includes deterministic, non-executing execution plan construction.

Current execution planning status:

- whole-graph execution planning: implemented
- dirty-subgraph execution planning: implemented
- schema-aware execution planning: implemented
- dependency/dependent step metadata: implemented
- runtime node evaluation: not implemented
- evaluator callbacks: not implemented
- stored execution state: not implemented
- threaded/asynchronous scheduling: not implemented

## Command layer status

WNG includes command-style mutation helpers that record mutation results for future undo/redo integration.

Current command status:

- graph command result records: implemented
- graph-only create/destroy command helpers: implemented
- schema-aware command helpers: implemented
- schema-defined instantiate-node command helper: implemented
- command batch records: implemented
- undo stack: not implemented
- redo stack: not implemented
- transaction execution: not implemented
- automatic rollback: not implemented

## Restoration status

WNG includes an object restoration helper for future undo/redo integration.

Current restoration status:

- restore captured node snapshots: implemented
- restore captured port snapshots: implemented
- restore captured link snapshots: implemented
- stable ID preservation during restore: implemented
- atomic restore through DTO replacement: implemented
- undo stack: not implemented
- redo stack: not implemented
- command replay: not implemented

## Undo/redo status

WNG includes command-record undo/redo application primitives, a minimal graph
command history owner, and a mixed graph-level history owner.

Current undo/redo status:

- command result records: implemented
- command batch records: implemented
- graph object restoration: implemented
- undo single command record: implemented
- undo command batch: implemented
- redo single command record: implemented
- redo command batch: implemented
- graph command history stacks: implemented
- mixed graph-level history stacks: implemented
- mixed graph command/schema migration undo ordering: implemented
- graph command transaction commit integration: implemented
- editor state undo: not implemented
- selection undo: not implemented
- transaction manager: not implemented
- automatic command recording: not implemented


## Command transaction status

WNG includes a minimal command transaction builder for grouping command results
before committing them to a history owner.

Current command transaction status:

- command result records: implemented
- command batch records: implemented
- command history stacks: implemented
- graph-level history stacks: implemented
- command transaction builder: implemented
- commit transaction to graph command history: implemented
- commit transaction to graph-level history: implemented
- rollback pending transaction through undo application: implemented
- automatic command execution: not implemented
- editor transaction manager: not implemented
- selection/editor-state transaction data: not implemented


## Graph diff status

WNG includes deterministic graph diffing for graph-core diagnostics and regression tests.

Current graph diff status:

- node added/removed/modified diffing: implemented
- port added/removed/modified diffing: implemented
- link added/removed/modified diffing: implemented
- stable-ID based object matching: implemented
- deterministic diff ordering: implemented
- schema diffing: not implemented
- patch application: not implemented
- merge/conflict resolution: not implemented
- editor visual diffing: not implemented


## Mutation preview status

WNG includes non-mutating previews for destructive graph mutations.

Current mutation preview status:

- preview destroy node consequences: implemented
- preview remove port consequences: implemented
- preview destroy link consequences: implemented
- preview summary ordering matches mutation summary ordering: implemented
- preview does not mutate graph state: implemented
- host consequence-preview callbacks: not implemented
- editor consequence UI: not implemented
- selection cleanup integration: not implemented


## Graph snapshot status

WNG includes an in-memory graph snapshot abstraction built on DTO import/export.

Current graph snapshot status:

- capture graph snapshot: implemented
- restore graph snapshot: implemented
- stable-ID preservation during restore: implemented
- atomic graph replacement during restore: implemented
- graph diff compatibility: implemented
- live graph to snapshot diffing: implemented
- snapshot to snapshot diffing: implemented
- file I/O: not implemented
- JSON serialization: not implemented
- binary serialization: not implemented
- editor state snapshots: not implemented
- selection snapshots: not implemented


## Schema snapshot status

WNG includes an in-memory schema snapshot abstraction for diagnostics and tests.

Current schema snapshot status:

- capture schema snapshot: implemented
- restore schema snapshot: implemented
- node definition preservation: implemented
- port definition preservation: implemented
- atomic schema replacement during restore: implemented
- schema-aware behavior after restore: implemented
- schema diffing: not implemented
- schema migration: not implemented
- file I/O: not implemented
- JSON serialization: not implemented
- editor schema drafts: not implemented

## Schema diff status

WNG includes deterministic schema diffing for schema diagnostics and tests.

Current schema diff status:

- node definition added/removed/modified diffing: implemented
- port definition added/removed/modified diffing: implemented
- stable node-type based matching: implemented
- stable port-definition matching by node type, port kind, and port name: implemented
- schema snapshot diffing: implemented
- deterministic diff ordering: implemented
- schema patch application: not implemented
- schema migration: not implemented
- schema merge/conflict resolution: not implemented
- schema persistence: not implemented

## Schema compatibility status

WNG includes read-only schema compatibility analysis for graph/schema diagnostics.

Current schema compatibility status:

- source-schema validation analysis: implemented
- target-schema validation analysis: implemented
- schema diff integration: implemented
- affected node reporting: implemented
- affected port reporting: implemented
- deterministic affected-ID ordering: implemented
- schema migration: not implemented
- automatic graph repair: not implemented
- schema patch application: not implemented
- editor migration UI: not implemented


## Schema migration planning status

WNG includes read-only schema migration planning for graph/schema diagnostics.

Current schema migration planning status:

- schema compatibility report consumption: implemented
- removed node type action planning: implemented
- modified node type action planning: implemented
- removed port definition action planning: implemented
- modified port definition action planning: implemented
- added required port action planning: implemented
- deterministic affected-ID reporting: implemented
- migration policy coverage reporting: implemented
- migration application: not implemented
- automatic graph repair: not implemented
- custom migration execution: not implemented
- editor migration UI: not implemented

## Schema migration policy status

WNG includes a value-oriented schema migration policy model for future migration workflows.

Current schema migration policy status:

- node type rename policy: implemented
- port definition rename policy: implemented
- port type change policy: implemented
- required port default policy: implemented
- node type removal acknowledgement: implemented
- port definition removal acknowledgement: implemented
- structural policy validation: implemented
- policy-aware migration planning: implemented
- schema-aware policy validation: implemented
- migration application: not implemented
- automatic graph repair: not implemented
- policy persistence: not implemented

## Schema migration apply preview status

WNG includes a read-only preview layer for schema migration application readiness.

Current schema migration apply preview status:

- schema-aware policy validation integration: implemented
- policy-aware migration planning integration: implemented
- uncovered blocking action reporting: implemented
- covered blocking action reporting: implemented
- non-blocking action reporting: implemented
- readiness classification: implemented
- migration application: not implemented
- automatic graph repair: not implemented
- command generation: not implemented
- editor migration UI: not implemented

## Schema migration command preview status

WNG includes a read-only preview of prospective schema migration operations.

Current schema migration command preview status:

- node type rename preview steps: implemented
- port definition rename preview steps: implemented
- port type change preview steps: implemented
- required port creation preview steps: implemented
- node removal preview steps: implemented
- port removal preview steps: implemented
- deterministic affected-ID reporting: implemented
- migration application: not implemented
- graph command generation: not implemented
- automatic graph repair: not implemented
- editor migration UI: not implemented

## Schema migration apply status

WNG includes atomic application for policy-covered schema migrations.

Current schema migration apply status:

- node type rename application: implemented
- port definition rename application: implemented
- port type metadata change application: implemented
- required port creation from policy defaults: implemented
- destructive node removal application: implemented
- destructive port removal application: implemented
- dependent link cleanup during destructive apply: implemented
- atomic graph replacement on success: implemented
- before/after graph diff reporting: implemented
- target schema validation after apply: implemented
- graph command generation: not implemented
- undo/redo history integration: not implemented
- editor migration UI: not implemented

## Schema migration apply command status

WNG includes a graph-level command wrapper for completed schema migrations.

Current schema migration apply command status:

- before graph snapshot capture: implemented
- after graph snapshot capture: implemented
- migration apply result preservation: implemented
- before/after graph diff preservation: implemented
- manual snapshot restore for future undo/redo: implemented
- GraphCommandHistory integration: not implemented
- GraphCommandRecord integration: not implemented
- automatic undo/redo integration: not implemented
- editor migration history: not implemented

## Schema migration apply command history status

WNG includes a dedicated history stack for schema migration apply command records.

Current schema migration apply command history status:

- migration apply command record stack ownership: implemented
- undo by restoring before graph snapshot: implemented
- redo by restoring after graph snapshot: implemented
- redo invalidation after new migration command record: implemented
- failed command record rejection: implemented
- GraphCommandHistory integration: not implemented
- GraphCommandRecord integration: not implemented
- mixed graph-command/migration-command history: implemented through GraphHistory
- editor migration history: not implemented
- selection/editor-state history: not implemented

## Graph-level history status

WNG includes a mixed graph-level history stack for user-level graph operations.

Current graph-level history status:

- normal graph command batch entries: implemented
- schema migration apply command entries: implemented
- graph command transaction commit integration: implemented
- mixed chronological undo/redo ordering: implemented
- redo invalidation after new mixed entry: implemented
- specialized GraphCommandHistory remains available: implemented
- specialized SchemaMigrationApplyCommandHistory remains available: implemented
- editor state history: not implemented
- selection state history: not implemented
- WPL/editor integration: not implemented

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Boundary

`wng_core` does not include WPL headers and has no dependency on WPL include paths.
WPL remains a future integration target outside WNG-0.1.
