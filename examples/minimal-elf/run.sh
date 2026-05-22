#!/usr/bin/env bash
set -euo pipefail

# LLM hint:
# - Deterministic local example for ELF + x86-64 self-hosted olnk flow.
# - Uses system compiler only to produce .o inputs; linking is done by olnk.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OLNK="${OLNK:-$ROOT/build-local/olnk}"
WORK="${WORK:-/tmp/olnk-example-minimal-elf}"

mkdir -p "$WORK"
cc -c "$ROOT/examples/minimal-elf/main.c" -o "$WORK/main.o"
cc -c "$ROOT/examples/minimal-elf/helper.c" -o "$WORK/helper.o"

"$OLNK" --format ELF --machine x86-64 -o "$WORK/out.elf" "$WORK/main.o" "$WORK/helper.o"

printf 'emitted %s (%s bytes)\n' "$WORK/out.elf" "$(stat -c '%s' "$WORK/out.elf")"
