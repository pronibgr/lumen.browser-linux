#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
cd "$DIR"

if [ ! -f "build/blueprint_browser" ]; then
    echo "[lampa] Compiling release build..."
    cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)
fi

echo "[lampa] Starting lampa browser (Linux Edition)..."
./build/blueprint_browser "$@"
