\page manual_ch04 Chapter 04 - Format System and Serialization
# Chapter 04 - Format System and Serialization

## 4.1 Objective
This chapter defines how output formats are represented, registered, validated, and executed through the public ABI. Formats are responsible for converting the host-prepared output model into concrete file bytes (ELF, PE, Mach-O, WASM container, raw image, etc.) without depending on private implementation details.

## 4.2 Contract surface
The sole contract for format plugins/adapters is `include/olnk/olnk-format.h`. Implementers should treat it as normative for:
- descriptor metadata;
- lifecycle callbacks;
- validation/defaulting hooks;
- serialization entry points;
- registry lookup behavior.

If a feature is not in this header, it must not be assumed or invented.

## 4.3 Descriptor model
A robust format descriptor should provide stable identity and capability statements:
- canonical name (`elf`, `pe`, `macho`, etc.);
- optional aliases, ordered deterministically;
- version metadata for human diagnostics;
- supported output kinds and capability bits;
- optional descriptive text for CLI/help surfaces.

Descriptors should use static-lifetime storage when built-in. Dynamically generated descriptors must preserve lifetime at least through registry ownership.

## 4.4 Lifecycle and instance ownership
Format instance lifecycle is strictly callback-driven:
1. host creates instance via ABI callback;
2. host provides context/config handles through declared callbacks;
3. format validates and optionally applies defaults;
4. format serializes from a documented image view;
5. host destroys instance.

Ownership rules:
- host owns host context/session buffers;
- format owns private instance state;
- format never frees host memory;
- format never stores dangling pointers to transient stack-backed host data.

## 4.5 Validation strategy
`validate_config` should be strict and deterministic. Validation should:
- reject unknown/ill-typed required options;
- report unsupported options as `NOT_SUPPORTED` rather than silently ignoring;
- normalize valid values into internal canonical form;
- emit concise diagnostics with key name and expected domain.

For partial implementations, validate known keys and return `NOT_IMPLEMENTED` for accepted-but-unwired advanced behavior.

## 4.6 Defaulting behavior
`apply_defaults` (if supported) should:
- fill only unset values;
- preserve explicit user choices;
- be deterministic and independent of host locale/time;
- avoid hidden environment-dependent heuristics.

Defaults should be documented in chapter text and mirrored in CLI help or script examples.

## 4.7 Section-name canonicalization
When `canonicalize_section_name` exists, define stable mapping rules:
- generic semantic class -> format-native section convention;
- deterministic case handling;
- fixed precedence for explicit names vs inferred names.

Example policy:
- code -> `.text`
- read-only data -> `.rodata`
- writable initialized data -> `.data`
- zero-initialized data -> `.bss`
- debug payloads -> format-specific debug namespace

## 4.8 Serialization responsibilities
`serialize` is the format’s critical function. It should:
- accept only documented image view contents;
- refuse opaque/internal assumptions not in ABI;
- write deterministic bytes for identical input model;
- return actionable status codes on failure;
- emit diagnostics that identify failing stage/sub-record.

If image view maturity is incomplete, serialization should fail conservatively with clear messages rather than guessing memory layout.

## 4.9 Registry behavior
Format registry invariants:
- deterministic registration order;
- deterministic alias resolution precedence;
- exact match and optional explicit case-fold policy (ASCII only unless documented);
- stable iteration for tooling/tests.

On lookup miss, return `NOT_FOUND` and leave out-parameters initialized.

## 4.10 Lua-backed format adapters
Lua integration (`src/olnk/lua_format.cpp`) should enforce a schema boundary:
- required callbacks exist and are callable;
- field types are validated before registration;
- Lua errors are trapped and mapped to status + diagnostics;
- adapter owns/refcounts Lua references safely;
- host remains stable when Lua code fails.

Lua modules should avoid direct assumptions about internal C++ classes; adapter must bridge to ABI-safe structures.

## 4.11 Testing matrix
Minimum tests:
- descriptor registration and deterministic iteration;
- alias lookup precedence;
- null argument and struct-size validation;
- option validation result mapping;
- deterministic serializer output for fixed model;
- failure-mode diagnostics and status codes.

Integration tests should include at least one successful and one intentionally failing serialization scenario per format family.

## 4.12 Evolution guidance
Format capabilities should be added incrementally and additively:
- keep existing callbacks backward compatible;
- add optional hooks rather than reinterpreting existing ones;
- gate new behavior behind explicit capability flags;
- preserve stable fallback paths.

The format layer is complete only when it can serialize supported targets deterministically and reject unsupported requests predictably.
