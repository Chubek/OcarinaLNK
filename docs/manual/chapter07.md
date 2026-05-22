\page manual_ch07 Chapter 07 - Plugin Architecture and Safety
# Chapter 07 - Plugin Architecture and Safety

## 7.1 Objective
Plugins allow optional analysis and transformation without bloating core linker paths. This chapter defines safe plugin integration that preserves ABI stability and deterministic host behavior.

## 7.2 ABI boundary
`include/olnk/olnk-plugin.h` is the plugin contract. Plugins must not:
- depend on unpublished host structures;
- mutate host memory outside declared callbacks;
- bypass lifecycle or version checks.

## 7.3 Discovery and registration
Plugin discovery may include static and dynamic sources. Invariants:
- registration order is deterministic;
- duplicate identity rules are explicit;
- incompatible plugins are rejected predictably;
- discovery failures are diagnosable and isolated.

## 7.4 Lifecycle discipline
Typical lifecycle:
1. discover descriptor;
2. validate ABI/version compatibility;
3. create instance;
4. invoke hook callbacks at defined pipeline stages;
5. collect diagnostics and outputs;
6. destroy instance.

Each transition should be status-coded and testable.

## 7.5 Failure isolation
Plugin failures should not collapse the linker unless policy requires fatal behavior:
- map plugin exceptions/errors to status + diagnostics;
- disable or skip failing plugin instance;
- continue pipeline for non-critical plugins;
- preserve deterministic failure order in reports.

## 7.6 Dynamic loading via dynalo
Dynamic plugin loading must verify:
- symbol presence and signatures;
- ABI version compatibility;
- descriptor validity and stable strings;
- unload safety (no dangling host callbacks).

Missing shared objects or bad symbols should produce `NOT_FOUND`/`NOT_SUPPORTED` style outcomes, not process termination.

## 7.7 Plugin data ownership
Ownership model:
- host owns session/context/IR view handles;
- plugin owns instance-private data;
- plugin outputs are returned via host APIs/callbacks;
- plugin must not free host-owned memory.

## 7.8 Deterministic plugin effects
Plugins that transform data (e.g., ICF) must be deterministic:
- stable candidate ordering;
- stable tie-breaking for equivalent folds;
- reproducible map/report formatting.

Read-only plugins should avoid side effects that reorder host diagnostics unpredictably.

## 7.9 Testing expectations
Tests should cover:
- static registration order;
- dynamic load success/failure cases;
- lifecycle callback sequencing;
- failure isolation behavior;
- deterministic report content/order.

## 7.10 Evolution guidance
Add new plugin hooks conservatively:
- prefer optional hook additions;
- preserve old plugin compatibility where practical;
- provide host capability checks;
- document hook ordering and reentrancy rules.
