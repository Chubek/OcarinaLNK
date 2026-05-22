# OcarinaLNK (olnk)

OcarinaLNK is a linker core with a stable C ABI and an internal C++ implementation.
It is designed for deterministic behavior, conservative ABI evolution, and extensibility
through Lua-defined formats/machines and C++ plugins.

## Project layout

- `include/olnk/*.h` - stable public C ABI
- `src/olnk/*.cpp` - core implementation
- `formats/*.lua` - Lua format definitions
- `machines/*.lua` - Lua machine definitions
- `plugins/*.cpp` - plugin implementations
- `scripts/*.lua` - user scripts and examples
- `tests/*` - unit/fuzz runners and test harnesses

## Current goals

1. ABI correctness and safety
2. Deterministic outputs for identical inputs/config
3. Conservative behavior for incomplete subsystems
4. Incremental implementation with clear upgrade paths

## Running tests

From repo root:

```sh
sh tests/run.sh
```

Azma smoke coverage currently runs:

- 40 unit parse checks
- 30 fuzz iterations (plus corpus seed runs)

You can also run the unit/fuzz wrappers directly:

```sh
sh tests/run_unit.sh
sh tests/run_fuzz.sh
```

## Notes for contributors

- Read `AGENTS.md` first for ABI and implementation rules.
- Do not invent public ABI beyond what `include/olnk/*.h` declares.
- Prefer deterministic conservative stubs over speculative behavior.

