#pragma once
#include <gtk/gtk.h>
#include <memory>
#include <vector>
#include <string>
#include "core/config.hpp"
#include "engine/web_tab.hpp"
#include "ui/topbar.hpp"

namespace Blueprint::Core {

class Application {
public:
    Application();
    ~Application();

    bool initialize(int argc, char* argv[]);
    void run();
    void shutdown();

private:
    bool m_running = false;
    bool m_zenMode = false;
    BrowserConfig m_config;

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

    UI::CompactTopbar m_topbar;

    void update();
    void syncTopbar();
    bool validActive() const;

    void createTab(const std::string& url = "lampa://newtab");
    void closeTab(int index);
    void switchTab(int oldIdx, int newIdx);
    void navigateActiveTab(const std::string& url);

    void zoomIn();
    void zoomOut();
    void resetZoom();
    void handleScrollZoom(double dy);

    void clearActiveSiteData();

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
};

} // namespace Blueprint::Core
