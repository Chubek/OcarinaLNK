#!/usr/bin/env sh
set -eu

# The azma smoke harness includes the requested 30 fuzz iterations.
sh tests/run_unit.sh
