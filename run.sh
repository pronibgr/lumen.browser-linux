#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
cd "$DIR"

cmake --build build -j$(nproc)

# Stability flags for GStreamer and WebKitGTK on Linux / NVIDIA Wayland
export __NV_DISABLE_EXPLICIT_SYNC=1
unset WEBKIT_DISABLE_DMABUF_RENDERER

echo "[lumen] Starting lumen browser (Linux Edition)..."
./build/blueprint_browser "$@"
