#include "core/app.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << " lampa browser (Linux Edition)\n";
    std::cout << " Theme: Deep Obsidian (#0E1116)\n";
    std::cout << " Wayland-Native / OpenGL Compositor\n";
    std::cout << " 180ms Directional Tab Transitions\n";
    std::cout << "========================================\n";

    Blueprint::Core::Application app;
    if (!app.initialize(argc, argv)) {
        std::cerr << "Failed to initialize lampa browser\n";
        return 1;
    }

    app.run();
    return 0;
}
