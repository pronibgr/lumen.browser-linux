#pragma once

#include <string>

namespace Blueprint::Installer {

struct InstallConfig {
    std::string themeName = "Obsidian Blueprint";
    std::string themeId = "obsidian_blueprint";
    std::string searchEngineName = "Brave Search";
    std::string searchEngineUrl = "https://search.brave.com/search?q=%s";
    bool onionEnabled = true;
    int torPort = 9050;
    bool createDesktopShortcut = true;
    bool setDefaultBrowser = true;
};

class InstallExecutor {
public:
    static bool deploy(const InstallConfig& config, std::string& outErrorMessage);
    static bool launchLumen(std::string& outErrorMessage);

private:
    static std::string findSourceBinary();
    static std::string findSourceAssets();
};

} // namespace Blueprint::Installer
