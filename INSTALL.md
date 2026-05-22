# INSTALL

This document covers practical setup for local development of OcarinaLNK.

## 1) Clone and enter repo

```sh
git clone <your-remote-url> ocarinalnk
cd ocarinalnk
```

## 2) Toolchain requirements

Minimum recommended tools:

- C/C++ compiler with C11/C++17 support (`cc`, `c++`, `clang`, or `gcc`)
- `bash` / POSIX shell
- `cmake` (recommended for future full build wiring)
- `rg` (ripgrep, optional but recommended)

## 3) Third-party dependencies

The repository already vendors multiple dependencies under `third_party/`.
No network install is required for the current smoke test path.

## 4) Verify local test harness

Run:

```sh
sh tests/run.sh
```

Expected output includes:

```text
AzmaTest run complete: unit_passed=40 fuzz_iterations=30 fuzz_runs=32
```

## 5) Troubleshooting

- If `Permission denied` appears for scripts, run with `sh` explicitly:
  - `sh tests/run.sh`
- If your compiler is not `cc`, set `CC` when needed:
  - `CC=clang sh tests/run_unit.sh`
- If includes are not found, ensure you run commands from repository root.

