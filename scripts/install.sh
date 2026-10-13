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
# Developer OAuth keys live outside the prefix: seed the user config dir
# on first install instead of embedding secrets into /usr.
if [ -f .env ] && [ ! -f "$HOME/.Versionary/.env" ]; then
  mkdir -p "$HOME/.Versionary"
  cp .env "$HOME/.Versionary/.env"
  chmod 600 "$HOME/.Versionary/.env"
  echo "Copied .env to ~/.Versionary/.env"
fi
echo "Installed to $PREFIX. OAuth keys resolve from env, build config, then ~/.Versionary/.env"
