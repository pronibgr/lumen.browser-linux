#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
cd "$DIR"

cmake --build build -j$(nproc)

# Stability flags for GStreamer and WebKitGTK on Linux / NVIDIA Wayland
export __NV_DISABLE_EXPLICIT_SYNC=1
unset WEBKIT_DISABLE_DMABUF_RENDERER

# Reload KWin rules on KDE to ensure borderless window rules are applied immediately
if [ -n "$KDE_FULL_SESSION" ] || [[ "$XDG_CURRENT_DESKTOP" == *"KDE"* ]]; then
    qdbus6 org.kde.KWin /KWin org.kde.KWin.reconfigure 2>/dev/null || qdbus org.kde.KWin /KWin org.kde.KWin.reconfigure 2>/dev/null || true
fi

echo "[lumen] Starting lumen browser (Linux Edition)..."
./build/blueprint_browser "$@"
