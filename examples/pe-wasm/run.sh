#!/usr/bin/env bash
set -euo pipefail
# LLM hint: PE alias + WASM alias deterministic smoke example.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OLNK="${OLNK:-$ROOT/build-local/olnk}"
WORK="${WORK:-/tmp/olnk-example-pe-wasm2}"
mkdir -p "$WORK"
cc -c "$ROOT/examples/pe-wasm/main.c" -o "$WORK/main.o"
cc -c "$ROOT/examples/pe-wasm/add.c" -o "$WORK/add.o"
"$OLNK" --format pe-coff --machine wasm32 -o "$WORK/out.pe" "$WORK/main.o" "$WORK/add.o"
printf 'ok %s %s\n' "$WORK/out.pe" "$(stat -c '%s' "$WORK/out.pe")"
