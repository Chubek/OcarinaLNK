# OcarinaLNK Documentation Frontend

Welcome to the generated documentation for **OcarinaLNK (olnk)**.

This page is the primary navigation hub for:
- the architecture of the linker and app surfaces;
- the full manual volume (`docs/manual/chapter01.md` to `chapter12.md`);
- practical guidance for browsing Doxygen output effectively.

OcarinaLNK is designed around a strict contract: a stable **C ABI** with an internal **C++ core**, plus controlled extension via **Lua** and **plugins**. Read the architecture and browsing sections below before diving into individual APIs.

## 1. Documentation map

### 1.1 Fast start paths
- **Need ABI contracts first?** Start in `include/olnk/*.h` from the "Files" and "File Members" pages.
- **Need runtime flow?** Read `src/olnk/linker.cpp`, then `output_builder.cpp`, `compute_offsets.cpp`, and `format_serializer.cpp`.
- **Need extension model?** Read Lua adapters (`src/olnk/lua_*.cpp`) and plugin APIs (`include/olnk/olnk-plugin.h`).

### 1.2 Manual chapters
- @subpage manual_ch01
- @subpage manual_ch02
- @subpage manual_ch03
- @subpage manual_ch04
- @subpage manual_ch05
- @subpage manual_ch06
- @subpage manual_ch07
- @subpage manual_ch08
- @subpage manual_ch09
- @subpage manual_ch10
- @subpage manual_ch11
- @subpage manual_ch12

## 2. App and library architecture guide

### 2.1 High-level layering
OcarinaLNK is intentionally layered:
1. **Public ABI layer** (`include/olnk/*.h`) - external contract and compatibility boundary.
2. **Core implementation layer** (`src/olnk/*.cpp`) - linker pipeline and domain logic.
3. **Adapter layer** (IR/format/machine/plugin bridges) - ABI-safe translation.
4. **Extension layer** (`formats/*.lua`, `machines/*.lua`, `plugins/*.cpp`) - optional capabilities.
5. **CLI app layer** (`src/main.cpp`, `src/olnk/cli.cpp`) - user-facing executable orchestration.

### 2.2 Main execution surfaces
- **Library usage**: host creates config/session, selects machine + format, runs pipeline through ABI entry points.
- **CLI usage**: CLI parses flags -> builds config -> invokes linker orchestration.
- **Format path**: pipeline prepares output image view -> selected format serializes final bytes.
- **Machine path**: relocation semantics and architecture policy resolved through machine callbacks.
- **Plugin path**: optional hooks observe/transform/report with failure isolation.

### 2.3 Determinism and safety principles
Across all components, the intended invariants are:
- deterministic iteration and output for identical inputs/config;
- strict ABI argument validation and output initialization;
- no C++ exception leakage across C ABI;
- explicit ownership boundaries for host/module/instance memory.

## 3. How to browse the generated Doxygen output

### 3.1 Best navigation order
Use this sequence when onboarding:
1. **Main Page** (this page) for map and constraints.
2. **Manual chapters** for policy and design intent.
3. **File List** for concrete headers and source entry points.
4. **Globals / File Members** for exported symbols and enums.
5. **Source Browser** for implementation specifics.

### 3.2 ABI-first browsing workflow
For any feature:
1. Locate the relevant header in `include/olnk`.
2. Identify structs, callbacks, and status contracts.
3. Open corresponding implementation in `src/olnk`.
4. Confirm deterministic and status-code behavior.
5. Cross-check related manual chapter for rationale.

### 3.3 Core file landmarks
- `src/olnk/api.cpp` - ABI-facing lifecycle and base entry points.
- `src/olnk/linker.cpp` - high-level pipeline coordinator.
- `src/olnk/format.cpp` / `src/olnk/machine.cpp` / `src/olnk/plugin.cpp` - registries and lookup.
- `src/olnk/format_serializer.cpp` - format invocation boundary.
- `src/olnk/ir.cpp` - public IR view adapters.
- `src/olnk/lua_format.cpp`, `src/olnk/lua_machine.cpp`, `src/olnk/lua_linker_script.cpp` - Lua bridge surfaces.

### 3.4 Extension browsing workflow
- **Formats**: start at `include/olnk/olnk-format.h`, then `src/olnk/format.cpp`, then Lua/C++ format providers.
- **Machines**: start at `include/olnk/olnk-machine.h`, then `src/olnk/machine.cpp` and relocation logic.
- **Plugins**: start at `include/olnk/olnk-plugin.h`, then `src/olnk/plugin.cpp`, then plugin implementations.

## 4. Manual usage guidance

The 12-chapter manual is organized from foundational constraints to long-term maintenance:
- Chapters 1-3: architecture contract, ABI/versioning, pipeline model.
- Chapters 4-8: format, machine, IR, plugin, and Lua extension systems.
- Chapters 9-12: diagnostics, determinism/performance, testing, governance.

For new contributors, recommended reading order is sequential. For focused tasks, jump directly to the subsystem chapter and then return to Chapter 2 for ABI consistency checks.

## 5. Notes for maintainers

When adding new files or APIs, update this page and the relevant manual chapter so Doxygen remains a complete navigational frontend, not just an index.
