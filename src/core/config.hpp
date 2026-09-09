#pragma once
#include <string>
#include <vector>

namespace Blueprint::Core {

struct BrowserConfig {
    std::string homePage = "lampa://newtab";
    std::string searchEngineUrl = "https://duckduckgo.com/?q=";
    bool zenMode = false;
    bool hardwareAcceleration = true;
    int windowWidth = 1280;
    int windowHeight = 800;

    // Chromium Zero-Telemetry isolation flags
    static std::vector<std::string> getChromiumFlags() {
        return {
            "--disable-background-networking",
            "--disable-sync",
            "--no-pings",
            "--disable-client-side-phishing-detection",
            "--disable-component-update",
            "--disable-domain-reliability",
            "--disable-features=Translate,AutofillServerCommunication,OptimizationHints"
        };
    }
};

} // namespace Blueprint::Core
