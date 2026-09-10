#include "core/app.hpp"
#include <iostream>
#include <stdlib.h>
#include <webkit2/webkit2.h>
#include <gst/gst.h>

int main(int argc, char* argv[]) {
    // Stability flags for GStreamer and WebKitGTK on Linux / NVIDIA Wayland:
    // 1. Prevents Error 71 / timeline fence crash on NVIDIA drivers with Wayland
    setenv("__NV_DISABLE_EXPLICIT_SYNC", "1", 1);
    // 2. Ensure broken WEBKIT_DISABLE_DMABUF_RENDERER is NOT set (it causes NULL deref in WebKit 2.52.6 on video playback)
    unsetenv("WEBKIT_DISABLE_DMABUF_RENDERER");

    // Initialize GStreamer before any WebKit threads/pipelines start
    gst_init(&argc, &argv);

    std::cout << "========================================\n";
    std::cout << " lumen browser (Linux Edition)\n";
    std::cout << " Theme: Deep Obsidian (#0E1116)\n";
    std::cout << " Wayland-Native / OpenGL Compositor\n";
    std::cout << " 180ms Directional Tab Transitions\n";
    std::cout << "========================================\n";

    Blueprint::Core::Application app;
    if (!app.initialize(argc, argv)) {
        std::cerr << "Failed to initialize lumen browser\n";
        return 1;
    }

    app.run();
    return 0;
}
