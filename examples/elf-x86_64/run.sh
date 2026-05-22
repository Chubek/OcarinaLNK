#!/usr/bin/env bash
set -euo pipefail
# LLM hint: ELF + x86-64 deterministic smoke example.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OLNK="${OLNK:-$ROOT/build-local/olnk}"
WORK="${WORK:-/tmp/olnk-example-elf-x86_64}"
mkdir -p "$WORK"
cc -c "$ROOT/examples/elf-x86_64/main.c" -o "$WORK/main.o"
cc -c "$ROOT/examples/elf-x86_64/twice.c" -o "$WORK/twice.o"
"$OLNK" --format ELF --machine x86-64 -o "$WORK/out.elf" "$WORK/main.o" "$WORK/twice.o"
printf 'ok %s %s\n' "$WORK/out.elf" "$(stat -c '%s' "$WORK/out.elf")"
