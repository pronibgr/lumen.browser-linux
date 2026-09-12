#include "installer/install_executor.hpp"
#include <glib.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sqlite3.h>
#include <unistd.h>
#include <sys/stat.h>

namespace Blueprint::Installer {

namespace fs = std::filesystem;

std::string InstallExecutor::findSourceBinary() {
    // 1. Look adjacent to current installer executable
    char exeBuf[PATH_MAX] = {0};
    ssize_t len = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
    if (len > 0) {
        fs::path exeDir = fs::path(std::string(exeBuf, len)).parent_path();
        if (fs::exists(exeDir / "lumen")) return (exeDir / "lumen").string();
        if (fs::exists(exeDir / "blueprint_browser")) return (exeDir / "blueprint_browser").string();
        if (fs::exists(exeDir / "build" / "lumen")) return (exeDir / "build" / "lumen").string();
        if (fs::exists(exeDir / "build" / "blueprint_browser")) return (exeDir / "build" / "blueprint_browser").string();
    }

    // 2. Look in current working directory and ./build
    if (fs::exists("./build/lumen")) return "./build/lumen";
    if (fs::exists("./build/blueprint_browser")) return "./build/blueprint_browser";
    if (fs::exists("./lumen")) return "./lumen";
    if (fs::exists("./blueprint_browser")) return "./blueprint_browser";

    return "";
}

std::string InstallExecutor::findSourceAssets() {
    char exeBuf[PATH_MAX] = {0};
    ssize_t len = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
    if (len > 0) {
        fs::path exeDir = fs::path(std::string(exeBuf, len)).parent_path();
        if (fs::exists(exeDir / "assets")) return (exeDir / "assets").string();
        if (fs::exists(exeDir.parent_path() / "assets")) return (exeDir.parent_path() / "assets").string();
    }

    if (fs::exists("./assets")) return "./assets";
    if (fs::exists("../assets")) return "../assets";

    return "";
}

static bool writeDbSettings(const fs::path& dbPath, const InstallConfig& config) {
    std::error_code ec;
    fs::create_directories(dbPath.parent_path(), ec);

    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.string().c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }

    const char* schema = "CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT NOT NULL);";
    char* err = nullptr;
    if (sqlite3_exec(db, schema, nullptr, nullptr, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        sqlite3_close(db);
        return false;
    }

    auto setKey = [db](const std::string& key, const std::string& val) {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?1, ?2);";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, val.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    };

    // Primary config keys specified in design
    setKey("theme_name", config.themeName);
    setKey("theme", config.themeId);
    setKey("search_engine_url", config.searchEngineUrl);
    setKey("search_engine_template", config.searchEngineUrl);
    setKey("search_engine_active_name", config.searchEngineName);
    setKey("search_engine_active_url", config.searchEngineUrl);
    setKey("search_engine_active", config.searchEngineName);
    setKey("onion_enabled", config.onionEnabled ? "1" : "0");
    setKey("onion_routing_enabled", config.onionEnabled ? "1" : "0");
    setKey("tor_port", std::to_string(config.torPort));
    setKey("onion_tor_port", std::to_string(config.torPort));

    sqlite3_close(db);
    return true;
}

bool InstallExecutor::deploy(const InstallConfig& config, std::string& outErrorMessage) {
    const char* homeEnv = std::getenv("HOME");
    if (!homeEnv || std::string(homeEnv).empty()) {
        outErrorMessage = "Environment variable HOME is not set.";
        return false;
    }
    fs::path home(homeEnv);
    std::error_code ec;

    // 1. Copy binaries and assets
    fs::path binDir = home / ".local" / "bin";
    fs::create_directories(binDir, ec);

    std::string srcBin = findSourceBinary();
    fs::path targetBin = binDir / "lumen";
    if (!srcBin.empty()) {
        fs::copy_file(srcBin, targetBin, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            outErrorMessage = "Failed to copy lumen binary: " + ec.message();
            return false;
        }
        fs::permissions(targetBin,
            fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
            fs::perms::others_read | fs::perms::others_exec,
            fs::perm_options::replace, ec);
    } else {
        outErrorMessage = "Could not locate source browser binary (lumen / blueprint_browser).";
        return false;
    }

    fs::path share_lumen_dir = home / ".local" / "share" / "lumen" / "assets";
    fs::create_directories(share_lumen_dir, ec);

    std::string srcAssets = findSourceAssets();
    if (!srcAssets.empty()) {
        fs::copy(srcAssets, share_lumen_dir, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    }

    // 2. Generate .desktop entry
    fs::path appsDir = home / ".local" / "share" / "applications";
    fs::create_directories(appsDir, ec);
    fs::path appDesktopFile = appsDir / "lumen.desktop";

    std::string desktopContent =
        "[Desktop Entry]\n"
        "Name=lumen\n"
        "Comment=Blueprint Browser with Strict Isolation\n"
        "Exec=" + targetBin.string() + " %U\n"
        "Icon=" + (share_lumen_dir / "logo.svg").string() + "\n"
        "Terminal=false\n"
        "Type=Application\n"
        "Categories=Network;WebBrowser;\n"
        "MimeType=text/html;text/xml;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;x-scheme-handler/onion;\n";

    {
        std::ofstream out(appDesktopFile);
        if (out.is_open()) {
            out << desktopContent;
        } else {
            outErrorMessage = "Failed to write applications .desktop entry.";
            return false;
        }
    }

    // Desktop shortcut if requested - using g_get_user_special_dir
    if (config.createDesktopShortcut) {
        const char* desktopSpecialDir = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
        if (desktopSpecialDir && fs::exists(desktopSpecialDir)) {
            fs::path userDesktopFile = fs::path(desktopSpecialDir) / "lumen.desktop";
            std::ofstream out(userDesktopFile);
            if (out.is_open()) {
                out << desktopContent;
                out.close();
                fs::permissions(userDesktopFile,
                    fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                    fs::perms::others_read | fs::perms::others_exec,
                    fs::perm_options::replace, ec);

                // Mark trusted in GNOME / FreeDesktop environment
                std::string cmd = "gio set \"" + userDesktopFile.string() + "\" metadata::trusted true 2>/dev/null";
                (void)system(cmd.c_str());
            }
        }
    }

    // 3. Write user configuration databases
    fs::path lumenDb = home / ".config" / "lumen" / "lumen.db";
    fs::path profileDb = home / ".config" / "lumen-browser" / "profile.db";
    writeDbSettings(lumenDb, config);
    writeDbSettings(profileDb, config);

    // 4. Default browser configuration
    if (config.setDefaultBrowser) {
        const char* xdgCmd = "xdg-settings set default-web-browser lumen.desktop";
        GError* gerr = nullptr;
        g_spawn_command_line_async(xdgCmd, &gerr);
        if (gerr) {
            g_error_free(gerr);
        }
    }

    return true;
}

bool InstallExecutor::launchLumen(std::string& outErrorMessage) {
    const char* homeEnv = std::getenv("HOME");
    std::string binPath;
    if (homeEnv) {
        fs::path installed = fs::path(homeEnv) / ".local" / "bin" / "lumen";
        if (fs::exists(installed)) {
            binPath = installed.string();
        }
    }

    if (binPath.empty()) {
        binPath = findSourceBinary();
    }

    if (binPath.empty()) {
        outErrorMessage = "lumen executable not found to launch.";
        return false;
    }

    char* argv[] = { const_cast<char*>(binPath.c_str()), nullptr };
    GError* err = nullptr;
    gboolean success = g_spawn_async(
        nullptr,
        argv,
        nullptr,
        G_SPAWN_SEARCH_PATH,
        nullptr,
        nullptr,
        nullptr,
        &err
    );

    if (!success) {
        outErrorMessage = err ? err->message : "Failed to spawn lumen process.";
        if (err) g_error_free(err);
        return false;
    }

    return true;
}

} // namespace Blueprint::Installer
