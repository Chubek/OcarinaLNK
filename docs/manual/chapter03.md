\page manual_ch03 Chapter 03 - Core Pipeline and Internal Domain Model
# Chapter 03 - Core Pipeline and Internal Domain Model

## 3.1 Scope
The internal linker pipeline coordinates object ingestion, symbol resolution, section normalization, relocation planning, layout computation, and final serialization. This chapter defines a practical, deterministic pipeline model without inventing public ABI beyond existing headers.

## 3.2 Pipeline stages
A conservative end-to-end flow:
1. parse user config and construct session;
2. ingest inputs into internal object representation;
3. collect and normalize sections;
4. construct symbol table and resolve bindings;
5. plan/apply relocations through machine rules;
6. compute addresses/offsets and build output image view;
7. invoke selected format serializer;
8. run optional plugins for reports/side effects.

Each stage should expose stable stage-local diagnostics.

## 3.3 Internal data model principles
Internal model may use C++ containers and helper libraries, but should preserve:
- explicit ownership;
- deterministic iteration fields (e.g., insertion index keys);
- stable canonical identifiers for sections/symbols.

Suggested internal entities:
- input file unit
- section record
- symbol record
- relocation record
- output segment/section plan
- emission buffer descriptor

## 3.4 Stage interfaces and handoff contracts
Treat each stage boundary as a mini-contract:
- ingress fields are validated;
- egress structures are normalized and self-consistent;
- ordering guarantees are explicit;
- failures contain actionable context and stage identity.

Avoid hidden cross-stage mutations where possible.

## 3.5 Deterministic ordering strategy
Ordering should not depend on hash bucket randomness or filesystem iteration order.

Common tactics:
- maintain explicit monotonically increasing insertion counters;
- sort by `(priority, canonical_name, insertion_id)`;
- for equal keys, preserve stable input order;
- keep plugin and registry order fixed at registration time.

## 3.6 Concurrency policy
Parallelism is allowed for pure compute tasks, but final state mutation should be committed in deterministic order.

Pattern:
- phase A: parallel parse/analyze into thread-local buffers;
- phase B: deterministic merge by stable key order;
- phase C: serial finalization and diagnostics emission.

## 3.7 Error containment and rollback
If a stage fails:
- stop dependent stages unless explicitly best-effort;
- preserve diagnostics already emitted;
- avoid partially committed global state that changes subsequent runs.

For optional stages (e.g., non-critical plugins), degrade gracefully with clear status.

## 3.8 Upgrade path from stubs to full behavior
Where stubs currently exist, add real behavior incrementally:
1. preserve API shape and status codes;
2. replace placeholder internals behind same ABI entry points;
3. add deterministic tests first, then implementation;
4. keep fallback path for unsupported edge cases.

## 3.9 Pipeline verification
Unit coverage should include:
- deterministic symbol ordering across permutations;
- section canonicalization invariants;
- relocation planning stability;
- repeatable layout offsets for identical inputs.

Integration coverage should include minimal sample links for each major format/machine combination.
