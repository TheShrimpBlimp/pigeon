#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
PIGEON_TEST_OUT=build/pigeon-tests/recovery
mkdir -p "$PIGEON_TEST_OUT"
PYTHON=${PYTHON:-python3}
CC=${CC:-cc}
"$PYTHON" tests/pigeon_recovery_packages.py
for name in pigeon_stub_test pigeon_recovery_test ldr_test ota_test; do
    "$CC" -O2 -w "tests/$name.c" -o "$PIGEON_TEST_OUT/$name"
done
"$PIGEON_TEST_OUT/pigeon_stub_test"
"$PIGEON_TEST_OUT/ota_test" build/pigeon.fwsc
"$PIGEON_TEST_OUT/ldr_test" "$PIGEON_TEST_OUT/stubless-baseline.fwsc" build/pigeon.fwsc
"$PIGEON_TEST_OUT/pigeon_recovery_test" "$PIGEON_TEST_OUT/stubless-baseline.fwsc" build/pigeon.fwsc "$PIGEON_TEST_OUT/changed-body.fwsc"
"$PYTHON" tests/install_test.py
node tests/pigeon_install_safety.mjs
