#!/bin/bash
# Script to build and test with sanitizers enabled

set -e

echo "Building with sanitizers (Address + UBSan)..."
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON -DBUILD_TESTS=ON -G Ninja
cmake --build build

echo "Running tests with sanitizers..."
./build/tests/vsdf_tests
./build/tests/filewatcher/filewatcher_tests

echo "All tests passed with sanitizers!"
