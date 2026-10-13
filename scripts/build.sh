#!/usr/bin/env bash
# Release build of Versionary into ./build.
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
echo "Binary: $(pwd)/build/Versionary"
