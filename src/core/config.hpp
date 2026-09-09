#pragma once
#include <string>
#include <vector>

namespace Blueprint::Core {

struct BrowserConfig {
    std::string homePage = "lumen://newtab";
    std::string searchEngineUrl = "https://duckduckgo.com/?q=";
    bool zenMode = false;
    bool hardwareAcceleration = true;
    int windowWidth = 1280;
    int windowHeight = 800;

    // format search query into url template replacing %s or appending
    static std::string formatSearchUrl(const std::string& tmpl, const std::string& query) {
        std::string encoded;
        encoded.reserve(query.size() * 3);
        for (unsigned char c : query) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~') {
                encoded.push_back(static_cast<char>(c));
            } else if (c == ' ') {
                encoded.push_back('+');
            } else {
                char hex[4];
                snprintf(hex, sizeof(hex), "%%%02X", c);
                encoded.append(hex);
            }
        }

        size_t pos = tmpl.find("%s");
        if (pos != std::string::npos) {
            std::string res = tmpl;
            res.replace(pos, 2, encoded);
            return res;
        }
        if (!tmpl.empty() && tmpl.back() == '=') {
            return tmpl + encoded;
        }
        return tmpl + "?q=" + encoded;
    }
};

} // namespace Blueprint::Core
