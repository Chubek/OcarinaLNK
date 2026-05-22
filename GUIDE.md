# GUIDE

This guide explains how to work in the OcarinaLNK repository safely and effectively.

## 1) Development principles

- Implement only what the public ABI declares in `include/olnk/*.h`.
- Keep behavior deterministic and reproducible.
- Use conservative stubs when architecture is incomplete.
- Initialize output structs and return stable status codes.
- Avoid exposing C++ internals through C ABI surfaces.

## 2) Where to make changes

- API/session/config behavior: `src/olnk/api.cpp`
- Format registry/lookup: `src/olnk/format.cpp`
- Format serialization bridge: `src/olnk/format_serializer.cpp`
- Machine registry/lookup: `src/olnk/machine.cpp`
- IR public adapter: `src/olnk/ir.cpp`
- Lua adapters: `src/olnk/lua_*.cpp`
- Plugin host/registry: `src/olnk/plugin.cpp`

## 3) Testing workflow

Primary local smoke run:

```sh
sh tests/run.sh
```

Direct runners:

```sh
sh tests/run_unit.sh
sh tests/run_fuzz.sh
```

Current smoke harness file:

- `tests/unittest/azma_smoke.c`

## 4) Recommended coding checklist

Before opening a change:

1. Read relevant header(s) in `include/olnk/`.
2. Confirm status-code behavior for invalid/null arguments.
3. Add/update concise LLM hint comments for non-obvious ABI decisions.
4. Run test scripts and confirm deterministic output.
5. Keep changes additive and avoid speculative subsystem design.

## 5) Common pitfalls

- Returning `OK` with uninitialized output structs.
- Adding behavior not represented by ABI callbacks/fields.
- Using nondeterministic ordering in registries/iteration.
- Letting exceptions cross C ABI boundaries.

## 6) Document updates

When behavior changes, update:

- `README.md` for project-level expectations
- `INSTALL.md` for setup/run commands
- this `GUIDE.md` for contributor workflow

