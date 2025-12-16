#!/bin/bash
# Script to build and run the fuzzer

set -e

echo "Building fuzzer..."
export CC=clang
export CXX=clang++

cmake -B build -DENABLE_FUZZING=ON -G Ninja
cmake --build build --target fuzz_target

echo "Running fuzzer for 60 seconds..."
./build/fuzz_target -max_total_time=60 -max_len=8192 fuzz_corpus/

echo "Fuzzing complete!"
