#pragma once
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include "core/config.hpp"
#include "engine/web_tab.hpp"
#include "ui/topbar.hpp"

namespace Blueprint::Core {

class Application;

class BrowserWindow {
public:
    BrowserWindow(Application* app, bool isEphemeral = false, const std::string& startUrl = "");
    ~BrowserWindow();

    bool initialize();
    GtkWidget* getWindow() const { return m_window; }
    bool isEphemeral() const { return m_isEphemeral; }

    void createTab(const std::string& url = "", bool switchToNewTab = true);
    void closeTab(int index);
    void switchTab(int oldIdx, int newIdx);
    void navigateActiveTab(const std::string& url);

    void zoomIn();
    void zoomOut();
    void resetZoom();
    void handleScrollZoom(double dy);
    void clearActiveSiteData();

    void update(float dt);
    void syncTopbar();
    bool validActive() const;

private:
    Application* m_app = nullptr;
    bool m_isEphemeral = false;
    std::string m_startUrl;
    WebKitWebContext* m_webContext = nullptr;

    bool m_zenMode = false;
    GtkWidget* m_window      = nullptr;
    GtkWidget* m_overlay     = nullptr;
    GtkWidget* m_box         = nullptr;
    GtkWidget* m_topbarArea  = nullptr;
    GtkWidget* m_stack       = nullptr;
    GtkWidget* m_overlayArea = nullptr;

    int m_winW = 1280, m_winH = 800;

    std::vector<std::shared_ptr<Engine::WebTab>> m_tabs;
    int m_activeIdx = 0;
    int m_nextId    = 1;
    guint m_heartbeatTimerId = 0;
    int m_themeListenerId = 0;

    UI::CompactTopbar m_topbar;

    std::chrono::steady_clock::time_point m_lastUpdateTime;
    bool m_hasLastUpdateTime = false;

    // GTK Callbacks
    static gboolean onWindowKeyPress(GtkWidget* widget, GdkEventKey* event, gpointer data);
    static gboolean onWindowScroll(GtkWidget* widget, GdkEventScroll* event, gpointer data);
    static gboolean onTopbarDraw(GtkWidget* widget, cairo_t* cr, gpointer data);
    static gboolean onTopbarMotion(GtkWidget* widget, GdkEventMotion* event, gpointer data);
    static gboolean onTopbarButtonPress(GtkWidget* widget, GdkEventButton* event, gpointer data);
    static gboolean onTopbarButtonRelease(GtkWidget* widget, GdkEventButton* event, gpointer data);
    static gboolean onTopbarScroll(GtkWidget* widget, GdkEventScroll* event, gpointer data);
    static gboolean onOverlayDraw(GtkWidget* widget, cairo_t* cr, gpointer data);
    static gboolean onOverlayMotion(GtkWidget* widget, GdkEventMotion* event, gpointer data);
    static gboolean onOverlayButtonPress(GtkWidget* widget, GdkEventButton* event, gpointer data);
    static gboolean onOverlayButtonRelease(GtkWidget* widget, GdkEventButton* event, gpointer data);
    static gboolean onOverlayScroll(GtkWidget* widget, GdkEventScroll* event, gpointer data);
};

class Application {
public:
    Application();
    ~Application();

    bool initialize(int argc, char* argv[]);
    void run();
    void shutdown();

    BrowserWindow* createWindow(bool isEphemeral = false, const std::string& startUrl = "");
    void removeWindow(BrowserWindow* win);
    const std::vector<std::unique_ptr<BrowserWindow>>& getWindows() const { return m_windows; }

private:
    bool m_running = false;
    std::vector<std::unique_ptr<BrowserWindow>> m_windows;
};

} // namespace Blueprint::Core
