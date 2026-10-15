#!/bin/sh
# Runs every benchmark and prints its result line, so a change that alters output is noticed.
# Time them with e.g. `time build/release/nuru bench/fib.nr` (build with -DCMAKE_BUILD_TYPE=Release).
nuru=${1:-build/release/nuru}
for f in "$(dirname "$0")"/*.nr; do
  printf '%-14s %s\n' "$(basename "$f")" "$("$nuru" "$f" 2>&1 | tail -n 1)"
done
