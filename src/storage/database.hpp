#pragma once
#include <string>
#include <vector>
#include <sqlite3.h>
#include <memory>
#include "omnibox/fuzzy_search.hpp"

namespace Blueprint::Storage {

struct HistoryEntry {
    int64_t id = 0;
    std::string url;
    std::string title;
    int64_t visitTime = 0;
    int visitCount = 1;
};

struct BookmarkEntry {
    int64_t id = 0;
    std::string url;
    std::string title;
    std::string folder;
    int64_t createdAt = 0;
};

class Database {
public:
    static Database& instance();

    bool initialize(const std::string& customPath = "");
    void close();

    // History
    bool addHistory(const std::string& url, const std::string& title);
    std::vector<HistoryEntry> getRecentHistory(int limit = 50);
    std::vector<Omnibox::SearchItem> searchHistory(const std::string& query, int limit = 20);

    // Bookmarks
    bool addBookmark(const std::string& url, const std::string& title, const std::string& folder = "Bookmarks");
    bool removeBookmark(const std::string& url);
    bool isBookmarked(const std::string& url);
    std::vector<BookmarkEntry> getBookmarks();

    // Settings
    bool setSetting(const std::string& key, const std::string& value);
    std::string getSetting(const std::string& key, const std::string& defaultValue = "");

private:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* m_db = nullptr;
    std::string getDatabasePath();
};

} // namespace Blueprint::Storage
