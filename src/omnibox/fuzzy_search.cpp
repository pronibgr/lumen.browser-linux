#include "omnibox/fuzzy_search.hpp"
#include <algorithm>
#include <cctype>

namespace Blueprint::Omnibox {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // anonymous namespace

int FuzzySearch::calculateScore(const std::string& pattern, const std::string& text) {
    if (pattern.empty()) return 0;
    if (text.empty()) return -1;

    std::string p = toLower(pattern);
    std::string t = toLower(text);

    // Exact match prefix bonus
    if (t.find(p) == 0) {
        return 1000 - static_cast<int>(t.length() - p.length());
    }

    // Substring match
    size_t subPos = t.find(p);
    if (subPos != std::string::npos) {
        return 500 - static_cast<int>(subPos * 10);
    }

    // Fuzzy subsequence match
    size_t pIdx = 0;
    int score = 0;
    int consecutive = 0;
    int prevMatchedIdx = -2;

    for (size_t tIdx = 0; tIdx < t.length() && pIdx < p.length(); ++tIdx) {
        if (t[tIdx] == p[pIdx]) {
            score += 10;
            if (static_cast<int>(tIdx) == prevMatchedIdx + 1) {
                consecutive++;
                score += consecutive * 15; // Consecutive characters bonus
            } else {
                consecutive = 0;
            }

            // Word boundary bonus
            if (tIdx == 0 || t[tIdx - 1] == ' ' || t[tIdx - 1] == '/' || t[tIdx - 1] == '.' || t[tIdx - 1] == '-' || t[tIdx - 1] == '_') {
                score += 25;
            }

            prevMatchedIdx = static_cast<int>(tIdx);
            pIdx++;
        }
    }

    if (pIdx == p.length()) {
        return score;
    }

    return -1; // Not matched
}

std::vector<SearchItem> FuzzySearch::rank(const std::string& query, const std::vector<SearchItem>& items, size_t maxResults) {
    if (query.empty()) {
        std::vector<SearchItem> result = items;
        if (result.size() > maxResults) result.resize(maxResults);
        return result;
    }

    std::vector<SearchItem> scoredItems;
    scoredItems.reserve(items.size());

    for (const auto& item : items) {
        int titleScore = calculateScore(query, item.title);
        int urlScore = calculateScore(query, item.url);
        int maxScore = std::max(titleScore, urlScore);

        if (maxScore > 0) {
            SearchItem scored = item;
            scored.score = maxScore;
            // Boost tabs over history
            if (scored.category == "tab") scored.score += 200;
            else if (scored.category == "bookmark") scored.score += 100;
            scoredItems.push_back(scored);
        }
    }

    std::sort(scoredItems.begin(), scoredItems.end(), [](const SearchItem& a, const SearchItem& b) {
        return a.score > b.score;
    });

    if (scoredItems.size() > maxResults) {
        scoredItems.resize(maxResults);
    }

    return scoredItems;
}

} // namespace Blueprint::Omnibox
