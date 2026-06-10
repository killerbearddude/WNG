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
selection, hit testing, serialization, undo/redo, replay, platform code, or X11.

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
- cycle validation: not yet implemented
- host validation callbacks: not yet implemented

## Traversal status

WNG includes deterministic, non-mutating traversal helpers.

Current traversal status:

- downstream reachability: implemented
- upstream reachability: implemented
- deterministic topological sort for acyclic graphs: implemented
- cycle reporting for topological sort: implemented
- cycle rejection as graph validation policy: not implemented
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
- undo stack: not implemented
- redo stack: not implemented
- transaction batching: not implemented

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Boundary

`wng_core` does not include WPL headers and has no dependency on WPL include paths.
WPL remains a future integration target outside WNG-0.1.
