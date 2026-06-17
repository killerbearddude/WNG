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
- host validation callbacks: implemented

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
- host validation callbacks: implemented
- schema connection callbacks in whole-graph schema validation: implemented

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
- schema diffing: implemented
- patch application: not implemented
- merge/conflict resolution: not implemented
- editor visual diffing: not implemented


## Schema migration status

WNG includes deterministic schema diagnostics and schema migration foundations for graph-core schema evolution.

Current schema migration status:

- schema snapshots: implemented
- schema diffing: implemented
- schema compatibility checks: implemented
- schema migration planning: implemented
- schema migration policy validation: implemented
- schema migration apply preview: implemented
- schema migration command preview: implemented
- schema migration apply: implemented
- schema migration apply with object removals: implemented
- schema migration apply command records: implemented
- schema migration apply command history: implemented
- file format migration: not implemented
- editor migration UI: not implemented
- visual migration diffing: not implemented
- automatic editor transaction manager: not implemented


## Mutation preview status

WNG includes non-mutating previews for destructive graph mutations.

Current mutation preview status:

- preview destroy node consequences: implemented
- preview remove port consequences: implemented
- preview destroy link consequences: implemented
- preview summary ordering matches mutation summary ordering: implemented
- preview does not mutate graph state: implemented
- host consequence-preview callbacks: implemented
- editor consequence UI: not implemented
- selection cleanup integration: not implemented
