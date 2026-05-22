\page manual_ch11 Chapter 11 - Testing, Validation, and Quality Gates
# Chapter 11 - Testing, Validation, and Quality Gates

## 11.1 Objective
This chapter defines quality expectations for unit, integration, and fuzz testing so that ABI safety and deterministic behavior remain continuously verified.

## 11.2 Test pyramid
Recommended balance:
- unit tests for isolated API/component contracts;
- integration tests for end-to-end flows (config -> link -> artifacts);
- fuzz tests for parser/adapter robustness;
- targeted regression tests for fixed bugs.

## 11.3 Unit-test priorities
Critical unit suites should cover:
- null and malformed argument handling;
- struct size/version compatibility;
- registry deterministic lookup/iteration;
- status code mapping;
- output struct initialization rules.

## 11.4 Integration-test priorities
Integration scenarios:
- minimal valid link per supported format/machine combination;
- Lua format/machine load and callback execution;
- plugin execution and non-fatal failure isolation;
- deterministic outputs and map files for identical inputs.

## 11.5 Fuzzing boundaries
Fuzz targets should focus on boundary-heavy interfaces:
- script parsing and schema validation;
- object/container metadata ingestion;
- serialization input validation;
- plugin/Lua adapter error paths.

Crashes are always bugs, even in unsupported-path handling.

## 11.6 Golden artifacts and reproducibility
When golden outputs are used:
- pin tool/input versions;
- compare deterministic fields only when metadata can vary;
- keep update process explicit and reviewed.

Prefer textual map/diagnostic snapshots for explainable diffs.

## 11.7 CI quality gates
Suggested gates:
- compile + unit test required;
- deterministic rerun check on selected integration samples;
- sanitizer/fuzz smoke for high-risk modules;
- lint/static checks where available.

## 11.8 Bug-to-test policy
Every substantive bug fix should add a test that fails pre-fix and passes post-fix. The new test should encode the root contract violation, not just the symptom.

## 11.9 Evolution guidance
As project maturity grows:
- increase matrix coverage gradually;
- keep tests fast and deterministic by default;
- isolate flaky or long-running suites behind explicit profiles.
