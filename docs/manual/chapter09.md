\page manual_ch09 Chapter 09 - Diagnostics, Logging, and Observability
# Chapter 09 - Diagnostics, Logging, and Observability

## 9.1 Objective
A linker must fail clearly and debug predictably. This chapter defines diagnostics strategy spanning ABI status codes, structured logging, and reproducible error reporting.

## 9.2 Layered error model
Three layers should remain distinct:
- status code: machine-readable API outcome;
- diagnostic message: human-readable explanation and context;
- debug trace/logging: optional detail for deep troubleshooting.

Do not substitute verbose logs for proper status-code mapping.

## 9.3 Status-code consistency
Equivalent failure classes should return identical status codes across modules. Diagnostics may differ in details, but top-level code should remain stable for automation.

## 9.4 Message quality standards
Good diagnostics are:
- specific (which stage/object/key failed);
- actionable (what user can change);
- deterministic (same input -> same message template);
- concise by default.

Avoid environment-sensitive noise such as nondeterministic addresses or thread IDs in default messages.

## 9.5 Logging backend guidance
When using `plog` + `fmt`:
- centralize formatting patterns;
- separate user-facing diagnostics from developer debug logs;
- make verbosity configurable;
- avoid global mutable formatting state that causes cross-test instability.

## 9.6 Extension failures
For Lua/plugin errors:
- include extension identifier and callback name;
- capture original error text safely;
- map to stable status;
- continue execution when failure is non-fatal by policy.

## 9.7 Deterministic ordering of diagnostics
If multiple errors appear, order them predictably by:
1. pipeline stage order;
2. canonical entity key;
3. stable insertion index.

This makes snapshot tests and CI diffs reliable.

## 9.8 Testing expectations
Add tests for:
- status code mapping per failure class;
- deterministic diagnostic text/ordering;
- null argument failures;
- extension error conversion behavior;
- debug-mode toggle not altering core status outcomes.

## 9.9 Evolution guidance
As diagnostics expand:
- keep base templates stable;
- add structured fields additively;
- document new diagnostic categories;
- preserve non-verbose defaults.
