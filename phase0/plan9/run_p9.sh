#!/bin/bash
# Phase 0.5 gate: every test (tests/ and tests_p9/) must give identical output natively and through kencc 6c.
P=$(cd "$(dirname "$0")" && pwd); pass=0; fail=0
for src in "$P"/../tests/*.cpp "$P"/../tests_p9/*.cpp; do
  if bash "$P/p9check.sh" "$src" 2>&1 | tee /dev/stderr | grep -q ': PASS'; then pass=$((pass+1)); else fail=$((fail+1)); fi
done 2>&1 | grep -E 'PASS|MISMATCH|FAIL'
