#include <cassert>
#include <iostream>
#include <filesystem>
#include <sqlite3.h>
#include <gtk/gtk.h>
#include <librsvg/rsvg.h>
#include <rlottie.h>
#include "installer/install_executor.hpp"
#include "installer/installer_window.hpp"
#include "installer/installer_steps.hpp"
#include "core/tor_bridge.hpp"

namespace fs = std::filesystem;

void testInstallConfigAndDb() {
    std::cout << "[Test] Running InstallConfig and database writing test..." << std::endl;
    Blueprint::Installer::InstallConfig cfg;
    cfg.themeName = "Noctiluca";
    cfg.themeId = "noctiluca";
    cfg.searchEngineName = "Brave Search";
    cfg.searchEngineUrl = "https://search.brave.com/search?q=%s";
    cfg.onionEnabled = true;
    cfg.torPort = 9050;
    cfg.createDesktopShortcut = false;
    cfg.setDefaultBrowser = false;

    // Test writing to a temporary test db
    fs::path tempDb = fs::temp_directory_path() / "test_lumen_installer.db";
    if (fs::exists(tempDb)) fs::remove(tempDb);

    sqlite3* db = nullptr;
    if (sqlite3_open(tempDb.string().c_str(), &db) != SQLITE_OK) std::exit(1);
    const char* schema = "CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT NOT NULL);";
    if (sqlite3_exec(db, schema, nullptr, nullptr, nullptr) != SQLITE_OK) std::exit(1);

    auto setKey = [db](const std::string& k, const std::string& v) {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO settings (key, value) VALUES (?1, ?2);", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, k.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, v.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    };

    setKey("theme_name", cfg.themeName);
    setKey("theme", cfg.themeId);
    setKey("search_engine_url", cfg.searchEngineUrl);
    setKey("onion_enabled", cfg.onionEnabled ? "1" : "0");
    setKey("tor_port", std::to_string(cfg.torPort));

    auto getKey = [db](const std::string& k) -> std::string {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, "SELECT value FROM settings WHERE key = ?1;", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, k.c_str(), -1, SQLITE_TRANSIENT);
        std::string res;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            res = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
        return res;
    };

    if (getKey("theme_name") != "Noctiluca") std::exit(1);
    if (getKey("theme") != "noctiluca") std::exit(1);
    if (getKey("search_engine_url") != "https://search.brave.com/search?q=%s") std::exit(1);
    if (getKey("onion_enabled") != "1") std::exit(1);
    if (getKey("tor_port") != "9050") std::exit(1);

    sqlite3_close(db);
    fs::remove(tempDb);
    std::cout << "  -> InstallConfig and database writing test PASSED!" << std::endl;
}

void testDesktopSpecialDir() {
    std::cout << "[Test] Verifying g_get_user_special_dir for Desktop..." << std::endl;
    const char* desktopDir = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
    assert(desktopDir != nullptr);
    std::string d(desktopDir);
    assert(!d.empty());
    std::cout << "  -> Desktop directory resolved to: " << d << " (PASSED!)" << std::endl;
}

void testInstallerThemesAndSearchEngines() {
    std::cout << "[Test] Verifying Theme definitions and Search Engines..." << std::endl;
    GtkWidget* dummyWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    Blueprint::Installer::InstallerSteps steps(GTK_WINDOW(dummyWin));

    const auto& themes = steps.getThemes();
    if (themes.size() != 10) {
        std::cerr << "Expected 10 themes, got " << themes.size() << std::endl;
        std::exit(1);
    }
    // 5 Dark themes
    if (themes[0].name != "Noctiluca" || !themes[0].isDark) std::exit(1);
    if (themes[1].name != "Morion" || !themes[1].isDark) std::exit(1);
    if (themes[2].name != "Scoria" || !themes[2].isDark) std::exit(1);
    if (themes[3].name != "Stibnite" || !themes[3].isDark) std::exit(1);
    if (themes[4].name != "Tephra" || !themes[4].isDark) std::exit(1);
    // 5 Light themes
    if (themes[5].name != "Calcite" || themes[5].isDark) std::exit(1);
    if (themes[6].name != "Rime" || themes[6].isDark) std::exit(1);
    if (themes[7].name != "Kaolin" || themes[7].isDark) std::exit(1);
    if (themes[8].name != "Selenite" || themes[8].isDark) std::exit(1);
    if (themes[9].name != "Loess" || themes[9].isDark) std::exit(1);

    // Verify step progression 0 -> 4
    for (int s = 0; s <= 4; ++s) {
        steps.setStep(s);
        if (steps.getCurrentStep() != s) std::exit(1);
    }

    gtk_widget_destroy(dummyWin);
    std::cout << "  -> All 10 Themes & Step Progression test PASSED!" << std::endl;
}

void testSearchEngineSvgs() {
    std::cout << "[Test] Verifying offline Search Engine vector SVGs..." << std::endl;
    const std::vector<std::string> icons = {
        "brave.svg", "duckduckgo.svg", "ecosia.svg", "startpage.svg", "bing.svg", "google.svg"
    };

    for (const auto& icon : icons) {
        std::vector<std::string> paths = {
            "assets/search_engines/" + icon,
            "../assets/search_engines/" + icon,
            "/home/elliot/Проекты/Blueprint Browser/assets/search_engines/" + icon
        };
        bool loaded = false;
        for (const auto& p : paths) {
            GError* err = nullptr;
            RsvgHandle* h = rsvg_handle_new_from_file(p.c_str(), &err);
            if (h) {
                loaded = true;
                g_object_unref(h);
                break;
            }
            if (err) g_error_free(err);
        }
        if (!loaded) {
            std::cerr << "Failed to load search SVG: " << icon << std::endl;
            std::exit(1);
        }
    }
    std::cout << "  -> All 6 Search Engine SVGs loaded successfully (PASSED!)" << std::endl;
}

void testLottieAnimations() {
    std::cout << "[Test] Verifying Lottie animations in assets/..." << std::endl;
    const std::vector<std::string> anims = {
        "wave.json", "star.json", "ringed_planet.json", "onion.json", "popper.json"
    };

    for (const auto& name : anims) {
        std::vector<std::string> paths = {
            "assets/" + name,
            "../assets/" + name,
            "/home/elliot/Проекты/Blueprint Browser/assets/" + name
        };
        bool loaded = false;
        for (const auto& p : paths) {
            auto a = rlottie::Animation::loadFromFile(p);
            if (a && a->totalFrame() > 0) {
                loaded = true;
                break;
            }
        }
        if (!loaded) {
            std::cerr << "Failed to load Lottie animation: " << name << std::endl;
            std::exit(1);
        }
    }
    std::cout << "  -> All 5 Lottie JSON animations loaded successfully (PASSED!)" << std::endl;
}

void testWindowProperties() {
    std::cout << "[Test] Verifying Frameless Window geometry & paintable flags..." << std::endl;
    Blueprint::Installer::InstallerWindow win;
    GtkWindow* w = win.getWindow();

    gint width = 0, height = 0;
    gtk_window_get_default_size(w, &width, &height);
    if (width != 1100 || height != 700) std::exit(1);

    if (gtk_window_get_decorated(w) != FALSE) std::exit(1);
    if (gtk_widget_get_app_paintable(GTK_WIDGET(w)) != TRUE) std::exit(1);

    std::cout << "  -> Window properties (1100x700, decorated=FALSE, app_paintable=TRUE) PASSED!" << std::endl;
}

void testTorSocketProbe() {
    std::cout << "[Test] Running Tor Socket Probe test..." << std::endl;
    // Port 1 should fail quickly without hanging
    bool probeFail = Blueprint::Core::TorBridge::probeTorDaemon(1, 100);
    if (probeFail) std::exit(1);
    std::cout << "  -> Tor Socket Probe test PASSED!" << std::endl;
}

void testCustomTitlebar() {
    std::cout << "[Test] Verifying Custom Titlebar & Vector Window Controls..." << std::endl;
    GtkWidget* dummyWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    Blueprint::Installer::InstallerTitlebar titlebar(GTK_WINDOW(dummyWin));
    assert(titlebar.getWidget() != nullptr);

    // Verify theme updates work cleanly for both dark and light palettes
    titlebar.updateTheme(true, "#F1F5F9", "#94A3B8");
    titlebar.updateTheme(false, "#0F172A", "#64748B");

    gtk_widget_destroy(dummyWin);
    std::cout << "  -> Custom Titlebar & Controls test PASSED!" << std::endl;
}

int main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);

    std::cout << "========================================" << std::endl;
    std::cout << " lumen setup wizard automated tests" << std::endl;
    std::cout << "========================================" << std::endl;

    testInstallConfigAndDb();
    testDesktopSpecialDir();
    testInstallerThemesAndSearchEngines();
    testSearchEngineSvgs();
    testLottieAnimations();
    testWindowProperties();
    testTorSocketProbe();
    testCustomTitlebar();

    std::cout << "========================================" << std::endl;
    std::cout << " ALL INSTALLER TESTS PASSED! ✅" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
