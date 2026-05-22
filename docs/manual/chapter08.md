\page manual_ch08 Chapter 08 - Lua Integration and Script Surfaces
# Chapter 08 - Lua Integration and Script Surfaces

## 8.1 Objective
Lua provides declarative and programmable extension for formats, machines, and user scripts. This chapter defines safe embedding patterns and schema discipline.

## 8.2 Runtime responsibilities
Lua runtime integration should:
- initialize and configure Lua state predictably;
- register only intended host APIs;
- isolate script errors from process stability;
- keep ownership/lifetime of Lua references explicit.

## 8.3 Schema validation
Lua modules must be validated before use:
- required fields present;
- callback values callable;
- enum-like strings normalized;
- numeric ranges checked;
- unknown critical keys handled consistently.

Validation failures should include module name, key path, and expected type.

## 8.4 Adapter contracts
Adapters (`lua_format.cpp`, `lua_machine.cpp`, `lua_linker_script.cpp`) should be thin translation layers:
- C ABI arguments -> Lua-safe representations;
- Lua return values -> ABI status and output fields;
- Lua errors -> deterministic diagnostics.

Avoid embedding core policy inside adapters when policy belongs to core pipeline.

## 8.5 Security and trust posture
Lua code may be project-local but should still be treated as untrusted extension input for robustness:
- validate all incoming script-driven options;
- avoid unchecked pointer/userdata flows;
- avoid hidden global mutable singleton state.

## 8.6 Determinism and script behavior
To preserve reproducibility:
- discourage nondeterministic APIs (time/random) in build-critical paths;
- if allowed, require explicit opt-in;
- keep module discovery and load order deterministic.

## 8.7 Error conversion
Any Lua exception or runtime error should be converted to:
- stable status code;
- concise diagnostic including callback/module context;
- safe defaulted out-parameters.

No uncaught script exception may cross ABI boundary.

## 8.8 Testing expectations
Include tests for:
- valid schema load for sample modules;
- missing required key failures;
- callback error mapping;
- deterministic script-driven configuration output;
- adapter behavior under null/invalid host arguments.

## 8.9 Evolution guidance
Add script surface area gradually:
- version or capability-gate new fields;
- document schema in manual + example scripts;
- keep backward-compatible defaults where feasible.
