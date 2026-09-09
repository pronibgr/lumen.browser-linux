#include "storage/database.hpp"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <cstdlib>

namespace Blueprint::Storage {

Database& Database::instance() {
    static Database db;
    return db;
}

Database::~Database() {
    close();
}

std::string Database::getDatabasePath() {
    const char* home = std::getenv("HOME");
    std::filesystem::path configDir;
    if (home) {
        configDir = std::filesystem::path(home) / ".config" / "lampa-browser";
        std::filesystem::path oldDir = std::filesystem::path(home) / ".config" / "blueprint-browser";
        if (!std::filesystem::exists(configDir) && std::filesystem::exists(oldDir)) {
            std::error_code ec;
            std::filesystem::copy(oldDir, configDir, std::filesystem::copy_options::recursive, ec);
        }
    } else {
        configDir = "./.lampa-browser";
    }
    std::filesystem::create_directories(configDir);
    return (configDir / "profile.db").string();
}

bool Database::initialize(const std::string& customPath) {
    if (m_db) return true;

    std::string path = customPath.empty() ? getDatabasePath() : customPath;
    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database] Cannot open sqlite database: " << sqlite3_errmsg(m_db) << std::endl;
        m_db = nullptr;
        return false;
    }

    // Initialize Schema
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            url TEXT NOT NULL UNIQUE,
            title TEXT NOT NULL,
            visit_time INTEGER NOT NULL,
            visit_count INTEGER NOT NULL DEFAULT 1
        );
        CREATE INDEX IF NOT EXISTS idx_history_url ON history(url);
        CREATE INDEX IF NOT EXISTS idx_history_title ON history(title);

        CREATE TABLE IF NOT EXISTS bookmarks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            url TEXT NOT NULL UNIQUE,
            title TEXT NOT NULL,
            folder TEXT DEFAULT 'Bookmarks',
            created_at INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )";

    char* errMsg = nullptr;
    rc = sqlite3_exec(m_db, schema, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database] Schema initialization failed: " << (errMsg ? errMsg : "") << std::endl;
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

void Database::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool Database::addHistory(const std::string& url, const std::string& title) {
    if (!m_db || url.empty()) return false;
    if (url == "lampa://newtab" || url == "blueprint://newtab" || url == "about:blank") return false;

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const char* sql = R"(
        INSERT INTO history (url, title, visit_time, visit_count)
        VALUES (?1, ?2, ?3, 1)
        ON CONFLICT(url) DO UPDATE SET
            title = CASE WHEN ?2 != '' THEN ?2 ELSE title END,
            visit_time = ?3,
            visit_count = visit_count + 1;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, now);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::vector<HistoryEntry> Database::getRecentHistory(int limit) {
    std::vector<HistoryEntry> entries;
    if (!m_db) return entries;

    const char* sql = "SELECT id, url, title, visit_time, visit_count FROM history ORDER BY visit_time DESC LIMIT ?1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return entries;

    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HistoryEntry e;
        e.id = sqlite3_column_int64(stmt, 0);
        e.url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        e.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        e.visitTime = sqlite3_column_int64(stmt, 3);
        e.visitCount = sqlite3_column_int(stmt, 4);
        entries.push_back(e);
    }
    sqlite3_finalize(stmt);
    return entries;
}

std::vector<Omnibox::SearchItem> Database::searchHistory(const std::string& query, int limit) {
    std::vector<Omnibox::SearchItem> results;
    if (!m_db) return results;

    const char* sql = R"(
        SELECT title, url FROM history
        WHERE title LIKE ?1 OR url LIKE ?1
        ORDER BY visit_count DESC, visit_time DESC
        LIMIT ?2;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    std::string pattern = "%" + query + "%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Omnibox::SearchItem item;
        item.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        item.url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        item.category = "history";
        results.push_back(item);
    }
    sqlite3_finalize(stmt);
    return results;
}

bool Database::addBookmark(const std::string& url, const std::string& title, const std::string& folder) {
    if (!m_db || url.empty()) return false;

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const char* sql = R"(
        INSERT INTO bookmarks (url, title, folder, created_at)
        VALUES (?1, ?2, ?3, ?4)
        ON CONFLICT(url) DO UPDATE SET title = ?2, folder = ?3;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, folder.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, now);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool Database::removeBookmark(const std::string& url) {
    if (!m_db || url.empty()) return false;
    const char* sql = "DELETE FROM bookmarks WHERE url = ?1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool Database::isBookmarked(const std::string& url) {
    if (!m_db || url.empty()) return false;
    const char* sql = "SELECT id FROM bookmarks WHERE url = ?1 LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

std::vector<BookmarkEntry> Database::getBookmarks() {
    std::vector<BookmarkEntry> entries;
    if (!m_db) return entries;

    const char* sql = "SELECT id, url, title, folder, created_at FROM bookmarks ORDER BY created_at DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return entries;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        BookmarkEntry e;
        e.id = sqlite3_column_int64(stmt, 0);
        e.url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        e.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        e.folder = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        e.createdAt = sqlite3_column_int64(stmt, 4);
        entries.push_back(e);
    }
    sqlite3_finalize(stmt);
    return entries;
}

bool Database::setSetting(const std::string& key, const std::string& value) {
    if (!m_db || key.empty()) return false;
    const char* sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?1, ?2);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::string Database::getSetting(const std::string& key, const std::string& defaultValue) {
    if (!m_db || key.empty()) return defaultValue;
    const char* sql = "SELECT value FROM settings WHERE key = ?1 LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return defaultValue;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    std::string val = defaultValue;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return val;
}

} // namespace Blueprint::Storage
