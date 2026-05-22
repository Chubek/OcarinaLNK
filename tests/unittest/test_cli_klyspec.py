#!/usr/bin/env python3
"""CLI integration checks for the Klyspec-driven frontend."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OLNK = ROOT / "build-local" / "olnk"


def run(args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(OLNK), *args],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


def require(cond: bool, msg: str) -> None:
    if not cond:
        raise AssertionError(msg)


def test_plugin_intercept_dry_run() -> None:
    proc = run(["--dry-run", "input.o"])
    require(proc.returncode == 0, f"expected 0, got {proc.returncode}: {proc.stderr}")
    require(
        "dry-run: parse/config validation complete; skipping linker session run" in proc.stdout,
        "dry-run intercept message missing",
    )


def test_profile_loading_success_and_failure() -> None:
    with tempfile.TemporaryDirectory() as td:
        profile = Path(td) / "ok.json"
        profile.write_text('{"output":"/tmp/out.bin","format":"ELF","machine":"x86-64"}', encoding="utf-8")

        ok = run(["--dry-run", "--profile", str(profile), "--profile-format", "json", "input.o"])
        require(ok.returncode == 0, f"profile success path failed: {ok.stderr}")

        bad = run(["--dry-run", "--profile", str(profile), "--profile-format", "invalid", "input.o"])
        require(bad.returncode == 2, f"invalid profile format should fail: {bad.returncode}")
        require("error: invalid profile format" in bad.stderr, "missing invalid format diagnostic")


def test_ipc_payload_determinism() -> None:
    args = ["--dry-run", "--ipc-signal", "--define", "a=1", "--define", "b=2", "input.o", "input2.o"]
    one = run(args)
    two = run(args)
    require(one.returncode == 0 and two.returncode == 0, "ipc determinism command failed")

    line1 = next((ln for ln in one.stdout.splitlines() if ln.startswith("ipc-payload:")), "")
    line2 = next((ln for ln in two.stdout.splitlines() if ln.startswith("ipc-payload:")), "")
    require(bool(line1), "missing ipc payload in first run")
    require(line1 == line2, "ipc payload differs between identical invocations")


def main() -> int:
    require(OLNK.exists(), f"missing executable: {OLNK}")
    test_plugin_intercept_dry_run()
    test_profile_loading_success_and_failure()
    test_ipc_payload_determinism()
    print("cli klyspec tests: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
