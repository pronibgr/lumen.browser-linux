#pragma once
#include <string>
#include <vector>

namespace Blueprint::Omnibox {

struct SearchItem {
    std::string title;
    std::string url;
    std::string category; // "tab", "history", "bookmark", "calc", "convert", "web"
    int score = 0;
};

class FuzzySearch {
public:
    static int calculateScore(const std::string& pattern, const std::string& text);
    static std::vector<SearchItem> rank(const std::string& query, const std::vector<SearchItem>& items, size_t maxResults = 8);
};

} // namespace Blueprint::Omnibox
