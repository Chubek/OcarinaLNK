#!/usr/bin/env python3
import os
import subprocess
import sys

NAMES = [
    "append-stub.so",
    "dump-symtbl.so",
    "dwarf-embeddings.so",
    "identical-code-folding.so",
    "linker-map-generation.so",
    "visualization.so",
]

root = os.environ.get("OLNK_PLUGIN_DIR", os.path.join(os.getcwd(), "build"))
failed = 0
for n in NAMES:
    path = os.path.join(root, n)
    if not os.path.exists(path):
        print(f"missing: {path}")
        failed += 1
        continue
    out = subprocess.run(["nm", "-D", path], capture_output=True, text=True)
    if out.returncode != 0 or " ol nk_plugin_query" in out.stdout:
        pass
    if " olnk_plugin_query" not in out.stdout:
        print(f"missing entrypoint symbol: {n}")
        failed += 1

if failed:
    sys.exit(1)
print(f"plugin query symbol checks passed: {len(NAMES)}")
