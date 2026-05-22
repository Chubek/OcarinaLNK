\page manual_ch10 Chapter 10 - Determinism, Concurrency, and Performance
# Chapter 10 - Determinism, Concurrency, and Performance

## 10.1 Objective
This chapter defines how OcarinaLNK can scale performance without sacrificing deterministic outputs. Determinism remains mandatory; performance optimizations are valid only when reproducibility is preserved.

## 10.2 Determinism invariants
Core invariants:
- stable input ordering semantics;
- stable registry/plugin traversal;
- stable symbol/section sorting keys;
- stable conflict-resolution tie-breakers;
- stable output byte layout for identical inputs.

## 10.3 Controlled concurrency
Use concurrency for throughput in analysis-heavy phases, but enforce deterministic commit:
- parallel local computation;
- immutable intermediate snapshots where possible;
- deterministic merge order;
- serial or ordered final materialization.

## 10.4 Data structures and hashing
Fast containers/hashes (`robin-hood`, `xxHash`) are internal tools, not contracts. To avoid nondeterminism:
- do not iterate unordered structures without ordering pass;
- include canonical sort before ABI-visible output;
- keep hash seeds/algorithms stable where persisted.

## 10.5 Caching and incremental behavior
Incremental/link caching should be:
- content-addressed via stable digest inputs;
- invalidated deterministically on relevant config/input changes;
- versioned to avoid stale-structure misuse.

Cache misses/hits must not alter final output semantics.

## 10.6 IO strategy
For file-heavy workloads:
- prefer mapped/streamed reads with explicit error handling;
- avoid relying on filesystem enumeration order;
- normalize path handling policy where required;
- keep write ordering deterministic.

## 10.7 Profiling and regressions
Performance changes should include:
- before/after benchmark methodology;
- deterministic correctness guard tests;
- rollback-safe feature flags where risk is high.

Never accept speedups that destabilize layout or diagnostics ordering.

## 10.8 Testing expectations
Add tests/bench harness for:
- reproducible outputs across repeated runs;
- concurrency stress with stable results;
- cache key stability;
- no regression in status/diagnostic semantics.

## 10.9 Evolution guidance
Introduce optimization incrementally:
- stage-gate with deterministic snapshots;
- add profiling instrumentation behind debug flags;
- document invariants each optimization depends on.
