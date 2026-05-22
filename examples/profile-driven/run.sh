#!/usr/bin/env bash
set -euo pipefail

# LLM hint:
# - Demonstrates CLI profile loading path (Klyspec-backed parser).
# - Profile keeps format/machine selection declarative and reproducible.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OLNK="${OLNK:-$ROOT/build-local/olnk}"
WORK="${WORK:-/tmp/olnk-example-profile}"
PROFILE="$ROOT/examples/profile-driven/profile.json"

mkdir -p "$WORK"
cc -c "$ROOT/examples/profile-driven/main.c" -o "$WORK/main.o"
cc -c "$ROOT/examples/profile-driven/mul.c" -o "$WORK/mul.o"

"$OLNK" --profile "$PROFILE" --profile-format json "$WORK/main.o" "$WORK/mul.o"

printf 'emitted %s (%s bytes)\n' "/tmp/olnk-example-profile/out.bin" "$(stat -c '%s' /tmp/olnk-example-profile/out.bin)"
