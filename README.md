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
- schema-aware graph mutation: not yet implemented
- host validation callbacks: not yet implemented

## Node type identity

Graph nodes carry a stable `type` string distinct from display title.

The `type` field is intended to identify the schema/domain node definition,
while `title` remains display-facing text.

Schema-aware graph mutation is not yet implemented.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Boundary

`wng_core` does not include WPL headers and has no dependency on WPL include paths.
WPL remains a future integration target outside WNG-0.1.
