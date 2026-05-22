\page manual_ch06 Chapter 06 - IR Views and Internal Builders
# Chapter 06 - IR Views and Internal Builders

## 6.1 Objective
This chapter describes the relationship between read-only public IR exposure and mutable internal construction. The key rule is strict separation: public ABI provides query/view contracts, while mutation/build details remain internal.

## 6.2 Public IR constraints
`include/olnk/olnk-ir.h` defines what callers can observe. It should be treated as a read-only lens over linker state. The ABI must not imply hidden mutability or undocumented traversal shortcuts.

## 6.3 Internal builder model
Internal builder modules (`ir_builder.cpp`, `output_builder.cpp`, etc.) may evolve quickly, but must not leak unstable representation details. Recommended builder principles:
- explicit stage-owned structures;
- deterministic record IDs;
- clear ownership transitions from parse -> normalize -> emit.

## 6.4 View adapter design
IR adapters should map internal records to stable ABI-facing structs:
- zero/initialize all output fields;
- validate caller struct sizes before writing;
- preserve safe lifetimes for referenced strings/buffers;
- avoid exposing mutable internals directly.

## 6.5 Conservative null-view stubs
If full IR extraction is unavailable, return conservative stubs:
- valid handle checks still enforced;
- empty iterators/counts returned deterministically;
- clear status (`NOT_IMPLEMENTED` or `NOT_SUPPORTED`);
- diagnostics explain feature boundary.

## 6.6 Iterator and traversal guarantees
IR iteration must define:
- deterministic order keys;
- stable begin/end semantics;
- behavior when container is empty;
- out-of-range handling without UB.

Never bind iteration order to hash table internals unless hash order is explicitly stabilized.

## 6.7 Data lifetime and snapshots
If IR data can change across pipeline phases, adapters should expose either:
- immutable snapshot semantics; or
- phase-scoped handles with explicit invalidation rules.

Ambiguous lifetime rules create subtle crashes and ABI misuse.

## 6.8 Diagnostics and failure mapping
Common IR failures:
- invalid handle -> `INVALID_ARGUMENT`
- unavailable feature path -> `NOT_IMPLEMENTED`
- missing record in valid domain -> `NOT_FOUND`

Diagnostics should mention IR entity type (section/symbol/relocation/image field) and lookup key.

## 6.9 Testing expectations
Unit tests should cover:
- empty/stub IR behavior;
- deterministic iterator ordering;
- struct-size compatibility handling;
- null/out-of-range argument responses;
- lifetime stability for returned string views.

## 6.10 Evolution guidance
As internal builder matures, extend adapters additively:
- retain existing query contracts;
- expose new fields via size/version expansion;
- keep fallback for old callers;
- document snapshot semantics explicitly.
