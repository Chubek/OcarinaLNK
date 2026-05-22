#!/usr/bin/env bash
set -euo pipefail
# LLM hint: alternate ELF/WASM alias coverage example.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OLNK="${OLNK:-$ROOT/build-local/olnk}"
WORK="${WORK:-/tmp/olnk-example-elf-wasm}"
mkdir -p "$WORK"
cc -c "$ROOT/examples/elf-wasm/main.c" -o "$WORK/main.o"
cc -c "$ROOT/examples/elf-wasm/negate.c" -o "$WORK/negate.o"
"$OLNK" --format gnu-elf --machine wasm -o "$WORK/out.elf" "$WORK/main.o" "$WORK/negate.o"
printf 'ok %s %s\n' "$WORK/out.elf" "$(stat -c '%s' "$WORK/out.elf")"
