#!/usr/bin/env bash
set -euo pipefail

# LLM / maintainer hints:
# - This test checks real linker behavior end-to-end (compile objects -> run olnk).
# - If emission fails, it compiles a tiny ABI probe to print olnk_result details.
# - Keep instrumentation deterministic and local to /tmp.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OLNK="$ROOT/build-local/olnk"
WORK="/tmp/olnk-real-smoke"

mkdir -p "$WORK"
cat > "$WORK/main.c" <<'EOF'
int add(int a, int b);
int main(void) { return add(2, 3) == 5 ? 0 : 1; }
EOF
cat > "$WORK/add.c" <<'EOF'
int add(int a, int b) { return a + b; }
EOF

cc -c "$WORK/main.c" -o "$WORK/main.o"
cc -c "$WORK/add.c" -o "$WORK/add.o"

rm -f "$WORK/a.out" "$WORK/a2.out" "$WORK/a3.out"
if ! "$OLNK" --format pe --machine wasm -o "$WORK/a.out" "$WORK/main.o" "$WORK/add.o" >/tmp/olnk-real-smoke.stdout 2>/tmp/olnk-real-smoke.stderr; then
  cat /tmp/olnk-real-smoke.stdout
  cat /tmp/olnk-real-smoke.stderr >&2
  exit 1
fi

if ! "$OLNK" --format PE --machine WASM -o "$WORK/a2.out" "$WORK/main.o" "$WORK/add.o" >/tmp/olnk-real-smoke2.stdout 2>/tmp/olnk-real-smoke2.stderr; then
  cat /tmp/olnk-real-smoke2.stdout
  cat /tmp/olnk-real-smoke2.stderr >&2
  exit 1
fi

if ! "$OLNK" --format pe-coff --machine wasm32 -o "$WORK/a3.out" "$WORK/main.o" "$WORK/add.o" >/tmp/olnk-real-smoke3.stdout 2>/tmp/olnk-real-smoke3.stderr; then
  cat /tmp/olnk-real-smoke3.stdout
  cat /tmp/olnk-real-smoke3.stderr >&2
  exit 1
fi

if [[ -f "$WORK/a.out" ]]; then
  size=$(stat -c '%s' "$WORK/a.out")
  size2=$(stat -c '%s' "$WORK/a2.out")
  size3=$(stat -c '%s' "$WORK/a3.out")
  cmp "$WORK/a.out" "$WORK/a2.out"
  cmp "$WORK/a.out" "$WORK/a3.out"
  echo "real linker smoke: deterministic output emitted ($WORK/a.out=${size}, $WORK/a2.out=${size2}, $WORK/a3.out=${size3})"
  exit 0
fi

cat > "$WORK/probe.cpp" <<'EOF'
#include <olnk/olnk-api.h>
#include <cstdio>
int main() {
  if (olnk_initialize() != OLNK_STATUS_OK) return 2;
  olnk_context_t* ctx = olnk_context_create();
  olnk_config_t* cfg = olnk_config_create();
  olnk_config_set_output_path(cfg, "/tmp/olnk-real-smoke/a.out");
  olnk_config_add_input_file(cfg, "/tmp/olnk-real-smoke/main.o");
  olnk_config_add_input_file(cfg, "/tmp/olnk-real-smoke/add.o");
  olnk_session_t* ses = olnk_session_create(ctx, cfg);
  olnk_result_t* res = nullptr;
  const olnk_status_t st = olnk_session_run(ses, &res);
  std::printf("session_run=%s\n", olnk_status_to_string(st));
  if (res != nullptr) {
    std::printf("result.status=%s\n", olnk_status_to_string(olnk_result_status(res)));
    std::printf("result.output_path=%s\n", olnk_result_output_path(res));
    std::printf("result.output_size=%llu\n", (unsigned long long)olnk_result_output_size(res));
    std::printf("result.inputs=%u warnings=%u errors=%u\n",
                olnk_result_input_count(res),
                olnk_result_warning_count(res),
                olnk_result_error_count(res));
  }
  const size_t dn = olnk_session_diagnostic_count(ses);
  std::printf("diagnostics=%zu\n", dn);
  for (size_t i = 0; i < dn; ++i) {
    const olnk_diagnostic_t* d = olnk_session_diagnostic_at(ses, i);
    std::printf("diag[%zu]=%s\n", i, olnk_diagnostic_message(d));
  }
  olnk_result_destroy(res);
  olnk_session_destroy(ses);
  olnk_config_destroy(cfg);
  olnk_context_destroy(ctx);
  olnk_shutdown();
  return 1;
}
EOF

g++ -std=c++17 -I"$ROOT/include" "$WORK/probe.cpp" "$ROOT/build-local/libolnk_core.a" -ldl -pthread -o "$WORK/probe"
"$WORK/probe"
