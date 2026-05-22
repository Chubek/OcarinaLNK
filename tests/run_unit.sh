#!/usr/bin/env sh
set -eu

cc -std=c11 -Wall -Wextra -I. \
  tests/unittest/azma_smoke.c third_party/AzmaTest/AzmaIDL.c \
  -o /tmp/azma_smoke_test

/tmp/azma_smoke_test

python3 tests/unittest/check_plugins.py
python3 tests/unittest/test_cli_klyspec.py
