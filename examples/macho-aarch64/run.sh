#!/usr/bin/env bash
set -euo pipefail
# LLM hint: Mach-O + Aarch64 deterministic smoke example.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OLNK="${OLNK:-$ROOT/build-local/olnk}"
WORK="${WORK:-/tmp/olnk-example-macho-aarch64}"
mkdir -p "$WORK"
cc -c "$ROOT/examples/macho-aarch64/main.c" -o "$WORK/main.o"
cc -c "$ROOT/examples/macho-aarch64/square.c" -o "$WORK/square.o"
"$OLNK" --format Mach-O --machine Aarch64 -o "$WORK/out.macho" "$WORK/main.o" "$WORK/square.o"
printf 'ok %s %s\n' "$WORK/out.macho" "$(stat -c '%s' "$WORK/out.macho")"
