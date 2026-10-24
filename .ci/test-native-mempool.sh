#!/usr/bin/env bash
# Compile the production pool header and regression tests with sanitizers.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
output="${1:-build/mempool-regression}"
gtest_root="${VITA_GOOGLETEST_ROOT:-external/googletest/googletest}"
mkdir -p "$output"
"${CXX:-c++}" -std=c++23 -O1 -g -pthread -fsanitize=address,undefined \
    -fno-omit-frame-pointer -fno-sanitize-recover=all \
    -I vita3k/mem/include -I vita3k/util/include \
    -I "$gtest_root/include" -I "$gtest_root" \
    vita3k/mem/tests/mempool_tests.cpp "$gtest_root/src/gtest-all.cc" \
    "$gtest_root/src/gtest_main.cc" -o "$output/mempool-tests"
# LeakSanitizer cannot inspect threads in some managed execution environments.
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$output/mempool-tests"
