\page manual_ch12 Chapter 12 - Roadmap, Governance, and Long-Term Maintenance
# Chapter 12 - Roadmap, Governance, and Long-Term Maintenance

## 12.1 Objective
This chapter defines how OcarinaLNK evolves from conservative scaffold to production-grade linker while preserving trust in ABI stability and reproducibility.

## 12.2 Phased roadmap discipline
Work should progress in explicit phases:
1. ABI-faithful foundation;
2. internal linking completeness;
3. Lua-backed extensibility;
4. plugin ecosystem hardening;
5. quality/tooling/performance maturity.

Each phase should have measurable exit criteria and regression protection.

## 12.3 Change classification
Classify every non-trivial change as:
- additive compatible;
- behavior-tightening;
- potentially breaking.

Potentially breaking changes require explicit design review, version strategy, and migration notes.

## 12.4 Governance checklist for reviews
Reviewers should ask:
- does this change alter ABI-observable behavior?
- are status codes and diagnostics stable?
- is deterministic ordering preserved?
- are ownership/lifetime semantics clear?
- are new tests sufficient and targeted?

## 12.5 Documentation as contract
Manual and header docs should evolve with code. For any new capability:
- update relevant chapter and examples;
- document fallback/stub behavior;
- describe compatibility implications.

Undocumented behavior should not be relied upon.

## 12.6 Dependency stewardship
Third-party dependencies should remain encapsulated:
- avoid leaking dependency types into ABI;
- pin/update thoughtfully with compatibility checks;
- keep optional dependencies optional where designed.

## 12.7 Release readiness criteria
A release is ready when:
- ABI tests and integration tests pass;
- deterministic artifact checks pass;
- known unsupported features fail cleanly;
- major extension paths (Lua/plugins) are stable under failure.

## 12.8 Incident response and regression handling
When regressions occur:
- reproduce with minimal deterministic case;
- classify contract violation (ABI, determinism, safety, perf);
- add failing test first;
- patch conservatively;
- document user-visible impact.

## 12.9 Long-term direction
Sustainable progress depends on conservative expansion:
- add capability through explicit extension points;
- keep internal refactors hidden behind stable wrappers;
- prioritize predictable behavior over speculative feature breadth.

This discipline ensures OcarinaLNK remains dependable for embedders and toolchain users over time.
