\page manual_ch01 Chapter 01 - Architecture and Design Contract
# Chapter 01 - Architecture and Design Contract

## 1.1 Why this manual exists
OcarinaLNK intentionally separates a stable C ABI from an internal C++ core and optional Lua/plugin extension layers. This split gives long-term binary compatibility to integrators while preserving implementation freedom for maintainers. The practical result is that every contribution has two responsibilities: improve functionality and preserve contract stability.

This chapter establishes the foundation for every other chapter in this manual:
- what is considered public contract versus private implementation detail;
- how deterministic behavior is treated as a product guarantee;
- what "conservative stubbing" means in practice;
- how the system can evolve without ABI drift.

## 1.2 Architecture at a glance
OcarinaLNK is organized into four strict layers:

1. Public ABI (`include/olnk/*.h`)
2. Core implementation (`src/olnk/*.cpp`)
3. Adapter layer (IR/format/machine/plugin bridges)
4. Extension layer (Lua modules and plugin binaries)

Only layer 1 is a compatibility contract for external users. Layers 2–4 are intentionally refactorable. A healthy review question is: "Would this change alter behavior visible through the headers?" If yes, it is an ABI-impacting change and must be handled explicitly.

## 1.3 Contract-first engineering
The public headers define all callable functions, structures, status enums, version constants, and ownership boundaries. The implementation must not create undocumented obligations for callers.

Contract-first rules:
- initialize output structures before returning from ABI functions;
- validate all required pointers and field sizes;
- return stable status codes for equivalent error classes;
- avoid hidden global state dependencies in lookup or iteration functions;
- never let C++ exceptions cross `extern "C"` boundaries.

## 1.4 Determinism as a product feature
For a linker, deterministic output is not optional. Reproducibility under identical inputs enables cache correctness, CI reliability, artifact auditing, and regression diagnosis.

Deterministic requirements include:
- stable registry order for formats, machines, and plugins;
- deterministic symbol and section visitation order;
- explicit normalization rules for names and aliases;
- deterministic diagnostics ordering for identical failure conditions;
- controlled concurrency where compute may parallelize but state commits are serialized.

## 1.5 Conservative stubbing policy
Incomplete subsystems should fail safely, not behave creatively. If architecture or ABI does not expose enough state for a feature, implement a narrow stub that is deterministic, documented, and upgrade-ready.

A compliant stub should:
- validate arguments and struct sizes;
- initialize out-parameters to known defaults;
- return `NOT_SUPPORTED`, `NOT_IMPLEMENTED`, `NOT_FOUND`, or `INVALID_ARGUMENT` as appropriate;
- provide concise diagnostics when the API supports host callbacks.

## 1.6 Extension model and trust boundaries
Lua modules and binary plugins extend behavior but must never weaken core guarantees. They operate through adapters that:
- validate schemas and callbacks;
- normalize errors into status codes;
- avoid exposing raw unstable internal pointers;
- preserve deterministic host behavior even on extension failure.

## 1.7 Definition of done for architecture-level work
Architecture-level work is complete when:
- ABI-facing behavior is explicitly verified with tests;
- stubs are deterministic and clearly marked for upgrade;
- ownership and lifetime semantics are documented in code comments;
- no hidden coupling to local environment, locale, or load order exists.

## 1.8 Practical checklist for contributors
Before merging a change, verify:
1. ABI headers remain source of truth.
2. All new ABI-returned structs are fully initialized.
3. New registry or traversal code has deterministic iteration.
4. Extension failures remain isolated and non-fatal where appropriate.
5. Tests include null/size/version boundary coverage.

This contract-first lens should remain active for every chapter that follows.
