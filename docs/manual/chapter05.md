\page manual_ch05 Chapter 05 - Machine Definitions and Relocation Policy
# Chapter 05 - Machine Definitions and Relocation Policy

## 5.1 Objective
Machine definitions encode architecture behavior: addressing model, endianness, alignment defaults, relocation rules, and output-kind constraints. This chapter describes how OcarinaLNK keeps machine behavior strict, deterministic, and ABI-safe.

## 5.2 ABI contract
Machine behavior is defined by `include/olnk/olnk-machine.h`. Implementations must:
- expose only declared callbacks and structures;
- initialize output structures consistently;
- avoid speculative architecture hooks outside ABI.

## 5.3 Descriptor essentials
A machine descriptor should include:
- stable canonical name and optional aliases;
- architecture class metadata;
- image class and endianness;
- default page/section alignment;
- capabilities (e.g., relocation support flags).

Alias sets and lookup behavior must be deterministic.

## 5.4 Relocation responsibility split
Keep policy boundaries clear:
- core pipeline decides *which* relocations require processing;
- machine logic decides *how* each relocation is interpreted/applied.

This split avoids format/core entanglement and keeps architecture differences localized.

## 5.5 Relocation mapping and normalization
Machine adapters should normalize relocation namespaces into stable internal identifiers:
- validate relocation code domain;
- map unknown codes to deterministic failure (`NOT_SUPPORTED` or `NOT_FOUND` per contract);
- avoid implicit fallback guessing.

Where different object syntaxes encode equivalent relocations, map them to a canonical internal form before planning.

## 5.6 Alignment and layout defaults
Machine defaults must be explicit and stable:
- page alignment;
- section alignment;
- minimum instruction/data alignment;
- ABI-constrained granularity.

Defaults should be deterministic constants, not host-platform probes.

## 5.7 Output-kind validation
Machine callback `validate_output_kind` (or equivalent) should verify compatibility among:
- selected machine;
- selected format;
- requested output type (executable/shared/object/raw).

Incompatible combinations must fail early with precise diagnostics.

## 5.8 Lua machine adapters
Lua bridge (`src/olnk/lua_machine.cpp`) should:
- validate module schema at load time;
- normalize numeric/enum fields into safe ranges;
- capture Lua runtime errors without crashing host;
- provide deterministic behavior for missing optional fields.

A broken Lua machine module should fail registration, not destabilize global registry.

## 5.9 Determinism in relocation application
Deterministic relocation behavior requires:
- stable traversal order by section/symbol/offset key;
- explicit tie-breaking where collisions are possible;
- consistent overflow/underflow policy;
- reproducible diagnostics for out-of-range values.

Parallel relocation analysis is allowed only with deterministic commit order.

## 5.10 Testing expectations
Unit tests should cover:
- machine lookup and alias resolution order;
- output kind acceptance/rejection matrix;
- relocation mapping for known codes;
- deterministic fixup bytes for fixed inputs;
- stable error results for invalid encodings.

Integration tests should link tiny architecture-targeted samples across supported machines.

## 5.11 Evolution guidance
Add architecture features incrementally:
- keep default behavior stable;
- introduce new relocation support through explicit map additions;
- maintain compatibility with earlier configs where possible;
- provide clear `NOT_IMPLEMENTED` stubs for planned extensions.
