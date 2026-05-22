#!/usr/bin/env bash
set -euo pipefail
# LLM hint: profile-driven alias resolution example.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OLNK="${OLNK:-$ROOT/build-local/olnk}"
WORK="${WORK:-/tmp/olnk-example-profile-aliased}"
mkdir -p "$WORK"
cc -c "$ROOT/examples/profile-aliased/main.c" -o "$WORK/main.o"
cc -c "$ROOT/examples/profile-aliased/id.c" -o "$WORK/id.o"
"$OLNK" --profile "$ROOT/examples/profile-aliased/profile.json" --profile-format json "$WORK/main.o" "$WORK/id.o"
printf 'ok %s %s\n' "/tmp/olnk-example-profile-aliased/out.bin" "$(stat -c '%s' /tmp/olnk-example-profile-aliased/out.bin)"
