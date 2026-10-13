#!/usr/bin/env bash
# Install Versionary into a prefix (default /usr/local).
# Usage: scripts/install.sh [prefix]   (DESTDIR also honored)
set -euo pipefail
cd "$(dirname "$0")/.."
PREFIX="${1:-/usr/local}"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build build -j"$(nproc)"
cmake --install build
echo "Installed to $PREFIX. OAuth keys go in ~/.Versionary/.env"
