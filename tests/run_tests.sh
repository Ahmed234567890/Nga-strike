#!/bin/sh
# Run the in-repo regression suite. Build first with `ninja check`.
set -eu

cd "$(dirname "$0")/.."

for test in tests/test_world tests/test_sites tests/test_settings tests/test_enemy_nav; do
    echo "== $test =="
    "$test"
done

echo "All regression tests passed."
