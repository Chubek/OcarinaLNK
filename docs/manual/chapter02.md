\page manual_ch02 Chapter 02 - Public ABI and Versioning Discipline
# Chapter 02 - Public ABI and Versioning Discipline

## 2.1 Scope
This chapter details the exact obligations around the public API surface under `include/olnk`. It covers versioning, structure sizing, ownership, error mapping, and binary compatibility practices.

## 2.2 Header families and intent
Key header roles:
- `olnk-api.h`: base status types, session/config lifecycle, common handles.
- `olnk-format.h`: format descriptors, vtables, lookup contracts.
- `olnk-machine.h`: architecture descriptors and machine callbacks.
- `olnk-ir.h`: read-only IR view/query interfaces.
- `olnk-plugin.h`: plugin descriptors, lifecycle, and discovery contracts.
- `olnk-version.h`: ABI/API version constants and compatibility markers.

Contributors should always read the relevant header first before touching implementation.

## 2.3 Structure versioning and size fields
Versioned structs are the primary compatibility mechanism. Required behavior:
- caller-provided `struct_size` must be validated before field access;
- implementation should fill only known fields and leave future fields untouched/zeroed;
- reserved pointers and padding must be initialized deterministically;
- `abi_version` fields must be populated accurately.

A safe pattern is to zero-initialize the entire output struct, then assign known fields.

## 2.4 Ownership and lifetime contracts
ABI code must make ownership explicit. Distinguish:
- host-owned: session/context/config handles created by API calls;
- module-owned static: descriptor strings and static definition tables;
- instance-owned: format/machine/plugin private state returned by create functions.

Prohibited behavior:
- returning pointers to stack temporaries;
- freeing memory owned by caller or host from extension code;
- aliasing mutable internal vectors directly through ABI.

## 2.5 Nullability and argument validation
Null acceptance is contract-specific. For each function:
- required pointers: return `INVALID_ARGUMENT` on null;
- optional pointers: tolerate null and maintain deterministic behavior;
- output pointers: never write through null; return `INVALID_ARGUMENT`.

When invalid input is detected, leave outputs initialized to safe defaults.

## 2.6 Status code mapping discipline
Status codes are API behavior, not implementation details. Equivalent failures must map to the same codes.

Recommended mapping:
- malformed arguments -> `INVALID_ARGUMENT`
- recognized but unsupported feature -> `NOT_SUPPORTED`
- planned but absent implementation -> `NOT_IMPLEMENTED`
- lookup miss in valid domain -> `NOT_FOUND`
- generic unexpected internal failure -> project-defined internal error code

Do not overload one status code for unrelated classes of failure.

## 2.7 Exception safety at the ABI boundary
Use explicit translation barriers:
- ABI wrappers should catch `std::exception` and unknown exceptions;
- convert to deterministic status values;
- optionally report concise diagnostics through host logging callbacks.

No exception should escape exported C symbols.

## 2.8 Compatibility workflow
Before changing headers:
1. identify whether change is additive or breaking;
2. for additive changes, append fields and preserve old layout;
3. for breaking changes, explicitly version bump and document migration;
4. extend tests for old and new struct sizes.

## 2.9 ABI review checklist
- Are all `extern "C"` exports stable?
- Do tests cover old struct size callers?
- Are reserved fields zeroed?
- Are outputs deterministic on failures?
- Does the change avoid leaking C++ types across ABI?

Maintaining this discipline is what keeps downstream tooling trust intact.
