#include "core/app.hpp"
#include "theme/colors.hpp"
#include "storage/database.hpp"
#include "omnibox/converter.hpp"
#include <iostream>
#include <algorithm>
#include <gdk/gdk.h>
#include <curl/curl.h>

namespace Blueprint::Core {

// ─────────────────────────────────────────────────────────────────────────────
// BrowserWindow Implementation
// ─────────────────────────────────────────────────────────────────────────────

BrowserWindow::BrowserWindow(Application* app, bool isEphemeral, const std::string& startUrl)
    : m_app(app), m_isEphemeral(isEphemeral), m_startUrl(startUrl) {
    if (m_isEphemeral) {
        m_webContext = webkit_web_context_new_ephemeral();
        m_topbar.setEphemeral(true);
    }
}

BrowserWindow::~BrowserWindow() {
    if (m_heartbeatTimerId) {
        g_source_remove(m_heartbeatTimerId);
        m_heartbeatTimerId = 0;
    }
    if (m_themeListenerId) {
        Theme::ThemeManager::instance().removeThemeListener(m_themeListenerId);
        m_themeListenerId = 0;
    }
    if (m_window) {
        GtkWidget* w = m_window;
        m_window = nullptr;
        gtk_widget_destroy(w);
    }
    if (m_isEphemeral && m_webContext) {
        g_object_unref(m_webContext);
        m_webContext = nullptr;
    }
}

bool BrowserWindow::initialize() {
    m_themeListenerId = Theme::ThemeManager::instance().addThemeListener([this](const Theme::Palette& pal) {
        for (auto& tab : m_tabs) {
            if (tab) tab->applyTheme(pal);
        }
    });

    m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(m_window), m_winW, m_winH);
    gtk_window_set_position(GTK_WINDOW(m_window), GTK_WIN_POS_CENTER);

    if (m_isEphemeral) {
        gtk_window_set_title(GTK_WINDOW(m_window), "l.null");
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        gtk_window_set_wmclass(GTK_WINDOW(m_window), "l.null", "l.null");
        #pragma GCC diagnostic pop
        gtk_window_set_icon_name(GTK_WINDOW(m_window), "l.null");
    } else {
        gtk_window_set_title(GTK_WINDOW(m_window), "lumen browser");
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        gtk_window_set_wmclass(GTK_WINDOW(m_window), "lumen-browser", "lumen-browser");
        #pragma GCC diagnostic pop
        gtk_window_set_icon_name(GTK_WINDOW(m_window), "lumen-browser");
    }

    // Apply browser logo as window and taskbar icon
    const char* iconCandidates[] = {
        "assets/logo.svg",
        "../assets/logo.svg"
    };
    GList* iconList = nullptr;
    for (const char* iconPath : iconCandidates) {
        GdkPixbuf* pb16 = gdk_pixbuf_new_from_file_at_scale(iconPath, 16, 16, TRUE, nullptr);
        GdkPixbuf* pb32 = gdk_pixbuf_new_from_file_at_scale(iconPath, 32, 32, TRUE, nullptr);
        GdkPixbuf* pb48 = gdk_pixbuf_new_from_file_at_scale(iconPath, 48, 48, TRUE, nullptr);
        GdkPixbuf* pb64 = gdk_pixbuf_new_from_file_at_scale(iconPath, 64, 64, TRUE, nullptr);
        GdkPixbuf* pb128 = gdk_pixbuf_new_from_file_at_scale(iconPath, 128, 128, TRUE, nullptr);
        GdkPixbuf* pb256 = gdk_pixbuf_new_from_file_at_scale(iconPath, 256, 256, TRUE, nullptr);
        if (pb16 && pb32 && pb48 && pb64 && pb128 && pb256) {
            iconList = g_list_append(iconList, pb16);
            iconList = g_list_append(iconList, pb32);
            iconList = g_list_append(iconList, pb48);
            iconList = g_list_append(iconList, pb64);
            iconList = g_list_append(iconList, pb128);
            iconList = g_list_append(iconList, pb256);
            gtk_window_set_icon_list(GTK_WINDOW(m_window), iconList);
            g_list_free_full(iconList, g_object_unref);
            break;
        } else {
            if (pb16) g_object_unref(pb16);
            if (pb32) g_object_unref(pb32);
            if (pb48) g_object_unref(pb48);
            if (pb64) g_object_unref(pb64);
            if (pb128) g_object_unref(pb128);
            if (pb256) g_object_unref(pb256);
            GError* err = nullptr;
            if (gtk_window_set_icon_from_file(GTK_WINDOW(m_window), iconPath, &err)) {
                break;
            }
            if (err) g_error_free(err);
        }
    }

    // Dark solid background for window and containers
    GtkCssProvider* cssProvider = gtk_css_provider_new();
    std::string appCss = m_isEphemeral ?
        "window, .background, box, stack, stack > * {\n"
        "    background-color: #070809;\n"
        "}\n" :
        "window, .background, box, stack, stack > * {\n"
        "    background-color: #0E1116;\n"
        "}\n";
    gtk_css_provider_load_from_data(cssProvider, appCss.c_str(), -1, nullptr);
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(m_window),
        GTK_STYLE_PROVIDER(cssProvider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(cssProvider);

    // Overlay allows drawing custom popups, certificate banners, and modals on top of WebViews
    m_overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(m_window), m_overlay);

    // Main vertical box: Topbar + Web Views
    m_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(m_overlay), m_box);

    // 1. Topbar Drawing Area (Height: Theme::TOPBAR_HEIGHT = 84px)
    m_topbarArea = gtk_drawing_area_new();
    gtk_widget_set_size_request(m_topbarArea, -1, Theme::TOPBAR_HEIGHT);
    gtk_widget_set_events(m_topbarArea, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
                                        GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);

    g_signal_connect(m_topbarArea, "draw", G_CALLBACK(onTopbarDraw), this);
    g_signal_connect(m_topbarArea, "motion-notify-event", G_CALLBACK(onTopbarMotion), this);
    g_signal_connect(m_topbarArea, "button-press-event", G_CALLBACK(onTopbarButtonPress), this);
    g_signal_connect(m_topbarArea, "button-release-event", G_CALLBACK(onTopbarButtonRelease), this);
    g_signal_connect(m_topbarArea, "scroll-event", G_CALLBACK(onTopbarScroll), this);
    gtk_box_pack_start(GTK_BOX(m_box), m_topbarArea, FALSE, FALSE, 0);

    // 2. Web View Stack (hosts tabs with smooth horizontal slide transition)
    m_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(m_stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(m_stack), 180);
    gtk_box_pack_start(GTK_BOX(m_box), m_stack, TRUE, TRUE, 0);

    // 3. Fullscreen Overlay Drawing Area (for Omnibox popup, Certificate Banner, Clear Data Modal, Settings)
    m_overlayArea = gtk_drawing_area_new();
    gtk_widget_set_events(m_overlayArea, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
                                         GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);
    g_signal_connect(m_overlayArea, "draw", G_CALLBACK(onOverlayDraw), this);
    g_signal_connect(m_overlayArea, "motion-notify-event", G_CALLBACK(onOverlayMotion), this);
    g_signal_connect(m_overlayArea, "button-press-event", G_CALLBACK(onOverlayButtonPress), this);
    g_signal_connect(m_overlayArea, "button-release-event", G_CALLBACK(onOverlayButtonRelease), this);
    g_signal_connect(m_overlayArea, "scroll-event", G_CALLBACK(onOverlayScroll), this);

    gtk_overlay_add_overlay(GTK_OVERLAY(m_overlay), m_overlayArea);
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(m_overlay), m_overlayArea, FALSE);
    gtk_widget_hide(m_overlayArea);

    // Window signals
    g_signal_connect(m_window, "key-press-event", G_CALLBACK(onWindowKeyPress), this);
    g_signal_connect(m_window, "scroll-event", G_CALLBACK(onWindowScroll), this);
    g_signal_connect(m_window, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
        auto* self = static_cast<BrowserWindow*>(data);
        self->m_window = nullptr; // prevent double destroy
        g_idle_add(+[](gpointer d) -> gboolean {
            auto* bwin = static_cast<BrowserWindow*>(d);
            bwin->m_app->removeWindow(bwin);
            return G_SOURCE_REMOVE;
        }, self);
    }), this);

    // Topbar callbacks
    m_topbar.getTabStrip().setCallbacks(
        [this](int oldIdx, int newIdx) { switchTab(oldIdx, newIdx); },
        [this]()                       { createTab(); },
        [this](int idx)                { closeTab(idx); }
    );

    m_topbar.getOmnibox().setOnNavigate([this](const std::string& url) {
        navigateActiveTab(url);
    });

    m_topbar.setOnBack([this]() {
        if (validActive()) { m_tabs[m_activeIdx]->goBack(); syncTopbar(); }
    });
    m_topbar.setOnForward([this]() {
        if (validActive()) { m_tabs[m_activeIdx]->goForward(); syncTopbar(); }
    });
    m_topbar.setOnReload([this]() {
        if (validActive()) { m_tabs[m_activeIdx]->reload(); }
    });
    m_topbar.setOnClearData([this]() {
        clearActiveSiteData();
    });
    m_topbar.setOnRefreshSiteData([this]() {
        if (validActive()) {
            m_tabs[m_activeIdx]->fetchWebsiteData();
        }
    });

    // Initialize WebTab default User-Agent from settings
    std::string initUa = m_topbar.getSettings().settings().getActiveUserAgent();
    Engine::WebTab::setDefaultUserAgent(initUa);

    m_topbar.getSettings().setOnUserAgentChanged([this](const std::string& ua) {
        Engine::WebTab::setDefaultUserAgent(ua);
        for (auto& tab : m_tabs) {
            if (tab) {
                tab->setUserAgent(ua);
            }
        }
    });

    std::string initUrl = m_startUrl;
    if (initUrl.empty()) {
        initUrl = m_isEphemeral ? "lumen://null" : "lumen://newtab";
    }
    createTab(initUrl);

    // VSync tick
    gtk_widget_add_tick_callback(m_window, +[](GtkWidget*, GdkFrameClock*, gpointer data) -> gboolean {
        auto* win = static_cast<BrowserWindow*>(data);
        auto now = std::chrono::steady_clock::now();
        float dt = 0.016f;
        if (win->m_hasLastUpdateTime) {
            float realDt = std::chrono::duration_cast<std::chrono::microseconds>(now - win->m_lastUpdateTime).count() / 1000000.f;
            dt = std::clamp(realDt, 0.001f, 0.050f);
        } else {
            win->m_hasLastUpdateTime = true;
        }
        win->m_lastUpdateTime = now;
        win->update(dt);
        return G_SOURCE_CONTINUE;
    }, this, nullptr);

    // Heartbeat fallback timer
    m_heartbeatTimerId = g_timeout_add(16, +[](gpointer data) -> gboolean {
        auto* win = static_cast<BrowserWindow*>(data);
        if (!win || !win->m_window || !GTK_IS_WIDGET(win->m_window) || !GTK_IS_WIDGET(win->m_topbarArea)) {
            return G_SOURCE_REMOVE;
        }
        if (win->m_topbar.wantsRedraw()) {
            gtk_widget_queue_draw(win->m_topbarArea);
            if (win->m_topbar.isAnyOverlayActive() && win->m_overlayArea && GTK_IS_WIDGET(win->m_overlayArea)) {
                gtk_widget_queue_draw(win->m_overlayArea);
            }
        }
        return G_SOURCE_CONTINUE;
    }, this);

    // Register internal media bridge: allows media played in any Lumen tab to be captured and controlled in the widget
    Engine::WebTab::setInternalMediaProvider([this]() -> std::string {
        for (const auto& tab : m_tabs) {
            if (tab && tab->isPlayingAudio() && tab->getUrl() != "lumen://newtab" && tab->getUrl() != "about:blank") {
                std::string t = tab->getTitle();
                if (t.empty()) t = "Lumen Media";
                std::string artist = "Lumen Web";
                size_t ytp = t.rfind(" - YouTube");
                if (ytp != std::string::npos) {
                    t = t.substr(0, ytp);
                    artist = "YouTube";
                }
                auto escapeJson = [](const std::string& s) {
                    std::string out;
                    for (char c : s) {
                        if (c == '"') out += "\\\"";
                        else if (c == '\\') out += "\\\\";
                        else if (c == '\n') out += "\\n";
                        else if (c == '\r') out += "\\r";
                        else out += c;
                    }
                    return out;
                };
                return "{\"hasPlayer\":true,\"player\":\"Lumen\",\"playbackStatus\":\"Playing\",\"title\":\"" +
                       escapeJson(t) + "\",\"artist\":\"" + escapeJson(artist) + "\",\"album\":\"\",\"artUrl\":\"\",\"position\":0,\"duration\":0}";
            }
        }
        return "";
    });

    Engine::WebTab::setInternalMediaCommander([this](const std::string& action, double param) -> bool {
        for (const auto& tab : m_tabs) {
            if (tab && tab->isPlayingAudio() && tab->getUrl() != "lumen://newtab") {
                std::string js;
                if (action == "playPause") {
                    js = "(() => { const v = document.querySelector('video, audio'); if (v) { if (v.paused) v.play(); else v.pause(); } })()";
                } else if (action == "setPosition") {
                    js = "(() => { const v = document.querySelector('video, audio'); if (v) { v.currentTime = " + std::to_string(static_cast<int>(param)) + "; } })()";
                } else if (action == "seek") {
                    js = "(() => { const v = document.querySelector('video, audio'); if (v) { v.currentTime += " + std::to_string(static_cast<int>(param)) + "; } })()";
                } else if (action == "next") {
                    js = "(() => { const v = document.querySelector('video, audio'); if (v) { v.currentTime += 10; } })()";
                } else if (action == "prev" || action == "previous") {
                    js = "(() => { const v = document.querySelector('video, audio'); if (v) { if (v.currentTime > 3) v.currentTime = 0; else v.currentTime -= 10; } })()";
                }
                if (!js.empty()) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                    webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(tab->getWebView()), js.c_str(), nullptr, nullptr, nullptr);
#pragma GCC diagnostic pop
                    return true;
                }
            }
        }
        return false;
    });

    gtk_widget_show_all(m_window);
    gtk_widget_hide(m_overlayArea);

    return true;
}

bool BrowserWindow::validActive() const {
    return m_activeIdx >= 0 && m_activeIdx < static_cast<int>(m_tabs.size());
}

void BrowserWindow::syncTopbar() {
    if (!validActive()) return;
    auto& tab = m_tabs[m_activeIdx];
    m_topbar.setActiveUrl(tab->getUrl());
    m_topbar.setNavState(tab->canGoBack(), tab->canGoForward(), tab->canReload());
    m_topbar.setTlsInfo(tab->getTlsInfo());
    m_topbar.setSiteData(tab->getSiteDataBytes(), tab->canClearData());
    m_topbar.setTabs(m_tabs, m_activeIdx);
    m_topbar.setLoadProgress(tab->getLoadProgress());
}

void BrowserWindow::createTab(const std::string& url, bool switchToNewTab) {
    std::string targetUrl = url;
    if (targetUrl.empty()) {
        targetUrl = m_isEphemeral ? "lumen://null" : "lumen://newtab";
    }

    int newId = m_nextId++;
    auto tab = std::make_shared<Engine::WebTab>(newId, targetUrl, "New Tab", m_webContext, m_isEphemeral);
    m_tabs.push_back(tab);

    std::string tabName = "tab_" + std::to_string(newId);
    gtk_stack_add_named(GTK_STACK(m_stack), tab->getWebView(), tabName.c_str());
    gtk_widget_show_all(tab->getWebView());

    // Tab signal callbacks
    tab->setCallbacks(
        [this, tab](const std::string&) {
            syncTopbar();
            gtk_widget_queue_draw(m_topbarArea);
        },
        [this, tab](const std::string& u) {
            if (validActive() && m_tabs[m_activeIdx] == tab) {
                syncTopbar();
                if (!m_isEphemeral) {
                    Storage::Database::instance().addHistory(u, tab->getTitle());
                }
            }
        },
        [this, tab](float prog) {
            if (validActive() && m_tabs[m_activeIdx] == tab) {
                m_topbar.setLoadProgress(prog);
                gtk_widget_queue_draw(m_topbarArea);
            }
        }
    );

    tab->setOnSiteDataChanged([this, tab](uint64_t) {
        if (validActive() && m_tabs[m_activeIdx] == tab) {
            syncTopbar();
            gtk_widget_queue_draw(m_topbarArea);
        }
    });

    tab->setOnNewTabRequested([this](const std::string& u, bool inBackground) {
        createTab(u, !inBackground);
    });

    tab->setOnFullscreenToggled([this](bool fs) {
        if (fs) {
            gtk_window_fullscreen(GTK_WINDOW(m_window));
            gtk_widget_hide(m_topbarArea);
        } else {
            gtk_window_unfullscreen(GTK_WINDOW(m_window));
            gtk_widget_show(m_topbarArea);
        }
    });

    if (switchToNewTab) {
        gtk_stack_set_visible_child_name(GTK_STACK(m_stack), tabName.c_str());
        int newIdx = static_cast<int>(m_tabs.size()) - 1;
        switchTab(m_activeIdx, newIdx);
    } else {
        syncTopbar();
        gtk_widget_queue_draw(m_topbarArea);
    }
}

void BrowserWindow::closeTab(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;

    auto tab = m_tabs[index];
    gtk_container_remove(GTK_CONTAINER(m_stack), tab->getWebView());
    m_tabs.erase(m_tabs.begin() + index);

    if (m_tabs.empty()) {
        if (m_window) {
            gtk_widget_destroy(m_window);
        }
        return;
    }

    m_activeIdx = std::clamp(m_activeIdx, 0, static_cast<int>(m_tabs.size()) - 1);
    std::string tabName = "tab_" + std::to_string(m_tabs[m_activeIdx]->getId());
    gtk_stack_set_visible_child_name(GTK_STACK(m_stack), tabName.c_str());
    syncTopbar();
    gtk_widget_queue_draw(m_topbarArea);
}

void BrowserWindow::switchTab(int oldIdx, int newIdx) {
    if (newIdx < 0 || newIdx >= static_cast<int>(m_tabs.size())) return;
    m_activeIdx = newIdx;

    std::string tabName = "tab_" + std::to_string(m_tabs[newIdx]->getId());
    if (newIdx == oldIdx) {
        gtk_stack_set_visible_child_name(GTK_STACK(m_stack), tabName.c_str());
        syncTopbar();
        gtk_widget_queue_draw(m_topbarArea);
        return;
    }

    GtkStackTransitionType transType = (newIdx > oldIdx)
        ? GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT
        : GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT;

    auto animSettings = m_topbar.getSettings().settings().anim;
    if (animSettings.enabled) {
        guint dur = static_cast<guint>(std::clamp(180.f / std::max(0.1f, animSettings.tabSlide), 50.f, 600.f));
        gtk_stack_set_transition_duration(GTK_STACK(m_stack), dur);
        gtk_stack_set_visible_child_full(GTK_STACK(m_stack), tabName.c_str(), transType);
    } else {
        gtk_stack_set_transition_duration(GTK_STACK(m_stack), 0);
        gtk_stack_set_visible_child_full(GTK_STACK(m_stack), tabName.c_str(), GTK_STACK_TRANSITION_TYPE_NONE);
    }

    syncTopbar();
    gtk_widget_queue_draw(m_topbarArea);

    if (m_tabs[newIdx]->getUrl() == "lumen://newtab") {
        m_tabs[newIdx]->syncSystemMediaState();
    }
}

void BrowserWindow::navigateActiveTab(const std::string& url) {
    if (!validActive()) return;
    m_tabs[m_activeIdx]->loadUrl(url);
    syncTopbar();
    gtk_widget_queue_draw(m_topbarArea);
}

void BrowserWindow::zoomIn() {
    if (validActive()) m_tabs[m_activeIdx]->zoomIn();
}

void BrowserWindow::zoomOut() {
    if (validActive()) m_tabs[m_activeIdx]->zoomOut();
}

void BrowserWindow::resetZoom() {
    if (validActive()) m_tabs[m_activeIdx]->resetZoom();
}

void BrowserWindow::handleScrollZoom(double dy) {
    if (validActive()) m_tabs[m_activeIdx]->handleScrollZoom(dy);
}

void BrowserWindow::clearActiveSiteData() {
    if (!validActive()) return;
    m_tabs[m_activeIdx]->clearWebsiteData([](bool success) {
        std::cout << "[Core] Cleared website data: " << (success ? "OK" : "Failed") << "\n";
    });
}

void BrowserWindow::update(float dt) {
    m_topbar.update(dt);

    bool overlayActive = m_topbar.isAnyOverlayActive();
    if (overlayActive != gtk_widget_get_visible(m_overlayArea)) {
        gtk_widget_set_visible(m_overlayArea, overlayActive);
    }

    if (m_topbar.wantsRedraw()) {
        gtk_widget_queue_draw(m_topbarArea);
        if (overlayActive) {
            gtk_widget_queue_draw(m_overlayArea);
        }
    }
}

// ─────────────────────────── GTK Signal Callbacks ──────────────────────────
gboolean BrowserWindow::onTopbarDraw(GtkWidget* widget, cairo_t* cr, gpointer data) {
    auto* self = static_cast<BrowserWindow*>(data);
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);
    self->m_topbar.draw(cr, w, h);
    return FALSE;
}

gboolean BrowserWindow::onTopbarMotion(GtkWidget* widget, GdkEventMotion* event, gpointer data) {
    auto* self = static_cast<BrowserWindow*>(data);
    if (self->m_topbar.handleMouseMove(event->x, event->y)) {
        gtk_widget_queue_draw(widget);
    }
    return TRUE;
}

gboolean BrowserWindow::onTopbarButtonPress(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    auto* self = static_cast<BrowserWindow*>(data);
    if (event->button == 1) {
        self->m_topbar.handleMouseDown(event->x, event->y);
        gtk_widget_queue_draw(widget);
        if (self->m_topbar.isAnyOverlayActive()) {
            gtk_widget_queue_draw(self->m_overlayArea);
        }
        return TRUE;
    } else if (event->button == 2) {
        self->m_topbar.getTabStrip().handleMouseDown(event->x, event->y, 2);
        self->m_topbar.getOmnibox().setFocused(false);
        gtk_widget_queue_draw(widget);
        if (self->m_topbar.isAnyOverlayActive()) {
            gtk_widget_queue_draw(self->m_overlayArea);
        }
        return TRUE;
    }
    return FALSE;
}

gboolean BrowserWindow::onTopbarButtonRelease(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    auto* self = static_cast<BrowserWindow*>(data);
    if (event->button == 1) {
        if (self->m_topbar.handleMouseUp(event->x, event->y)) {
            gtk_widget_queue_draw(widget);
            if (self->m_topbar.isAnyOverlayActive()) {
                gtk_widget_queue_draw(self->m_overlayArea);
            }
            return TRUE;
        }
    }
    return FALSE;
}

gboolean BrowserWindow::onTopbarScroll(GtkWidget* widget, GdkEventScroll* event, gpointer data) {
    auto* self = static_cast<BrowserWindow*>(data);
    double dx = 0.0;
    if (event->direction == GDK_SCROLL_UP)   dx = -1.0;
    else if (event->direction == GDK_SCROLL_DOWN) dx = 1.0;
    else if (event->direction == GDK_SCROLL_SMOOTH) dx = event->delta_y;

    if (self->m_topbar.handleMouseWheel(dx)) {
        gtk_widget_queue_draw(widget);
        return TRUE;
    }
    return FALSE;
}

gboolean BrowserWindow::onOverlayDraw(GtkWidget* widget, cairo_t* cr, gpointer data) {
    auto* self = static_cast<BrowserWindow*>(data);
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);
    self->m_topbar.drawOverlays(cr, w, h);
    return FALSE;
}

gboolean BrowserWindow::onOverlayMotion(GtkWidget* widget, GdkEventMotion* event, gpointer data) {
    auto* self = static_cast<BrowserWindow*>(data);
    bool changed = self->m_topbar.handleMouseMove(event->x, event->y);
    if (changed) {
        gtk_widget_queue_draw(widget);
        gtk_widget_queue_draw(self->m_topbarArea);
    }
    return TRUE;
}

gboolean BrowserWindow::onOverlayButtonPress(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    auto* self = static_cast<BrowserWindow*>(data);
    if (event->button == 1) {
        self->m_topbar.handleMouseDown(event->x, event->y);
        gtk_widget_queue_draw(widget);
        gtk_widget_queue_draw(self->m_topbarArea);
        return TRUE;
    } else if (event->button == 2) {
        self->m_topbar.getTabStrip().handleMouseDown(event->x, event->y, 2);
        self->m_topbar.getOmnibox().setFocused(false);
        gtk_widget_queue_draw(widget);
        gtk_widget_queue_draw(self->m_topbarArea);
        return TRUE;
    }
    return FALSE;
}

gboolean BrowserWindow::onOverlayButtonRelease(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    auto* self = static_cast<BrowserWindow*>(data);
    if (event->button == 1) {
        if (self->m_topbar.handleMouseUp(event->x, event->y)) {
            gtk_widget_queue_draw(widget);
            gtk_widget_queue_draw(self->m_topbarArea);
            return TRUE;
        }
    }
    return FALSE;
}

gboolean BrowserWindow::onOverlayScroll(GtkWidget* widget, GdkEventScroll* event, gpointer data) {
    auto* self = static_cast<BrowserWindow*>(data);
    double dy = 0.0;
    if (event->direction == GDK_SCROLL_UP)   dy = -1.0;
    else if (event->direction == GDK_SCROLL_DOWN) dy = 1.0;
    else if (event->direction == GDK_SCROLL_SMOOTH) dy = event->delta_y;

    if (self->m_topbar.getSettings().handleScroll(dy)) {
        gtk_widget_queue_draw(widget);
        return TRUE;
    }
    return FALSE;
}

gboolean BrowserWindow::onWindowKeyPress(GtkWidget*, GdkEventKey* event, gpointer data) {
    auto* self = static_cast<BrowserWindow*>(data);
    bool ctrl = (event->state & GDK_CONTROL_MASK) != 0;
    bool shift = (event->state & GDK_SHIFT_MASK) != 0;

    // determine normalized latin key for shortcuts regardless of layout
    guint latinKeyval = event->keyval;
    if (ctrl) {
        if (event->hardware_keycode == 38) latinKeyval = GDK_KEY_a;
        else if (event->hardware_keycode == 54) latinKeyval = GDK_KEY_c;
        else if (event->hardware_keycode == 55) latinKeyval = GDK_KEY_v;
        else if (event->hardware_keycode == 53) latinKeyval = GDK_KEY_x;
        else if (event->hardware_keycode == 52) latinKeyval = GDK_KEY_z;
        else if (event->hardware_keycode == 22) latinKeyval = GDK_KEY_BackSpace;
        else if (event->hardware_keycode == 17) latinKeyval = GDK_KEY_w;
        else if (event->hardware_keycode == 28) latinKeyval = GDK_KEY_t;
        else if (event->hardware_keycode == 57) latinKeyval = GDK_KEY_n;
        else if (event->hardware_keycode == 46) latinKeyval = GDK_KEY_l;
        else if (event->hardware_keycode == 27) latinKeyval = GDK_KEY_r;
    }

    GdkDisplay* display = gdk_display_get_default();
    GdkKeymap* keymap = display ? gdk_keymap_get_for_display(display) : nullptr;
    if (keymap) {
        guint translated = 0;
        if (gdk_keymap_translate_keyboard_state(
                keymap,
                event->hardware_keycode,
                static_cast<GdkModifierType>(event->state),
                0,
                &translated,
                nullptr, nullptr, nullptr)) {
            if (translated != 0 && (translated < 0x0400 || translated > 0x06ff)) {
                latinKeyval = translated;
            }
        }
    }

    // ──────────────── Ctrl + Shift + N: Open Ephemeral l.null Window ──────────
    if (ctrl && shift && (latinKeyval == GDK_KEY_n || latinKeyval == GDK_KEY_N || event->hardware_keycode == 57)) {
        self->m_app->createWindow(/*isEphemeral=*/true, "lumen://null");
        return TRUE;
    }

    uint32_t effectiveState = event->state | (ctrl ? 4 : 0);
    guint keyToSend = ctrl ? latinKeyval : event->keyval;

    // 1. settings modal gets top priority
    if (self->m_topbar.getSettings().isVisible()) {
        self->m_topbar.getSettings().handleKeyPress(keyToSend, effectiveState, event->string);
        gtk_widget_queue_draw(self->m_overlayArea);
        return TRUE;
    }

    // 2. Escape exits video fullscreen if active
    if (event->keyval == GDK_KEY_Escape) {
        if (self->m_window) {
            GdkWindow* gdkWin = gtk_widget_get_window(self->m_window);
            if (gdkWin && (gdk_window_get_state(gdkWin) & GDK_WINDOW_STATE_FULLSCREEN)) {
                gtk_window_unfullscreen(GTK_WINDOW(self->m_window));
                gtk_widget_show(self->m_topbarArea);
                return TRUE;
            }
        }
    }

    // 3. clear site data / cert banner modals dismiss on escape
    if (self->m_topbar.isAnyOverlayActive() && event->keyval == GDK_KEY_Escape) {
        self->m_topbar.handleKeyPress(event->keyval, effectiveState, nullptr);
        gtk_widget_queue_draw(self->m_overlayArea);
        gtk_widget_queue_draw(self->m_topbarArea);
        return TRUE;
    }

    // 4. omnibox handling when focused
    if (self->m_topbar.getOmnibox().isFocused()) {
        if (self->m_topbar.handleKeyPress(keyToSend, effectiveState, event->string)) {
            gtk_widget_queue_draw(self->m_topbarArea);
            if (self->m_topbar.isAnyOverlayActive()) {
                gtk_widget_queue_draw(self->m_overlayArea);
            }
            return TRUE;
        }
    }

    // 5. browser global shortcuts
    if (ctrl) {
        guint k = latinKeyval;
        // new tab shortcut
        if (k == GDK_KEY_n || k == GDK_KEY_N ||
            k == GDK_KEY_t || k == GDK_KEY_T) {
            self->createTab();
            return TRUE;
        }
        // close active tab
        if (k == GDK_KEY_w || k == GDK_KEY_W) {
            self->closeTab(self->m_activeIdx);
            return TRUE;
        }
        // focus the url bar
        if (k == GDK_KEY_l || k == GDK_KEY_L) {
            self->m_topbar.getOmnibox().setFocused(true);
            gtk_widget_queue_draw(self->m_topbarArea);
            return TRUE;
        }
        // cycle tabs forward
        if (event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_ISO_Left_Tab) {
            if (!self->m_tabs.empty()) {
                int next = (self->m_activeIdx + 1) % static_cast<int>(self->m_tabs.size());
                self->switchTab(self->m_activeIdx, next);
            }
            return TRUE;
        }
        // zoom in
        if (k == GDK_KEY_plus || k == GDK_KEY_equal || k == GDK_KEY_KP_Add) {
            self->zoomIn();
            return TRUE;
        }
        // zoom out
        if (k == GDK_KEY_minus || k == GDK_KEY_KP_Subtract) {
            self->zoomOut();
            return TRUE;
        }
        // reset zoom back to 100%
        if (k == GDK_KEY_0 || k == GDK_KEY_KP_0) {
            self->resetZoom();
            return TRUE;
        }
    }

    // zen mode fullscreen hide
    if (event->keyval == GDK_KEY_F11) {
        self->m_zenMode = !self->m_zenMode;
        gtk_widget_set_visible(self->m_topbarArea, !self->m_zenMode);
        return TRUE;
    }

    // reload: F5 or Ctrl + R
    if (event->keyval == GDK_KEY_F5 || ((latinKeyval == GDK_KEY_r || latinKeyval == GDK_KEY_R) && ctrl)) {
        if (self->validActive()) {
            auto tab = self->m_tabs[self->m_activeIdx];
            if (tab && tab->canReload()) {
                tab->reload();
            }
        }
        return TRUE;
    }

    // Shift + T: close current active tab
    if (!self->m_topbar.getOmnibox().isFocused() && shift && !ctrl &&
        (event->keyval == GDK_KEY_T || event->keyval == GDK_KEY_t)) {
        self->closeTab(self->m_activeIdx);
        return TRUE;
    }

    return FALSE;
}

gboolean BrowserWindow::onWindowScroll(GtkWidget*, GdkEventScroll* event, gpointer data) {
    auto* self = static_cast<BrowserWindow*>(data);
    if (event->state & GDK_CONTROL_MASK) {
        double dy = 0.0;
        if (event->direction == GDK_SCROLL_UP)   dy = 1.0;
        else if (event->direction == GDK_SCROLL_DOWN) dy = -1.0;
        else if (event->direction == GDK_SCROLL_SMOOTH) dy = -event->delta_y;

        self->handleScrollZoom(dy);
        return TRUE;
    }

    if (self->m_topbar.getSettings().isVisible()) {
        double dy = 0.0;
        if (event->direction == GDK_SCROLL_UP)   dy = -1.0;
        else if (event->direction == GDK_SCROLL_DOWN) dy = 1.0;
        else if (event->direction == GDK_SCROLL_SMOOTH) dy = event->delta_y;

        if (self->m_topbar.getSettings().handleScroll(dy)) {
            gtk_widget_queue_draw(self->m_overlayArea);
            return TRUE;
        }
    }
    return FALSE;
}

// ─────────────────────────────────────────────────────────────────────────────
// Application Implementation
// ─────────────────────────────────────────────────────────────────────────────

Application::Application() {}
Application::~Application() { shutdown(); }

bool Application::initialize(int argc, char* argv[]) {
    // Stability flags for GPU / Wayland
    setenv("__NV_DISABLE_EXPLICIT_SYNC", "1", 1);
    unsetenv("WEBKIT_DISABLE_DMABUF_RENDERER");

    curl_global_init(CURL_GLOBAL_ALL);

    if (!gtk_init_check(&argc, &argv)) {
        std::cerr << "[Core] GTK initialization failed\n";
        return false;
    }

    Storage::Database::instance().initialize();

    g_set_prgname("lumen-browser");
    g_set_application_name("lumen browser");

    std::string startUrl = "lumen://newtab";
    if (argc > 1 && argv[1] && argv[1][0] != '\0') {
        startUrl = argv[1];
    }

    // Create the primary browser window
    createWindow(/*isEphemeral=*/false, startUrl);

    m_running = true;
    return true;
}

BrowserWindow* Application::createWindow(bool isEphemeral, const std::string& startUrl) {
    auto win = std::make_unique<BrowserWindow>(this, isEphemeral, startUrl);
    if (!win->initialize()) {
        return nullptr;
    }
    auto* rawPtr = win.get();
    m_windows.push_back(std::move(win));
    return rawPtr;
}

void Application::removeWindow(BrowserWindow* win) {
    auto it = std::find_if(m_windows.begin(), m_windows.end(), [win](const auto& ptr) {
        return ptr.get() == win;
    });
    if (it != m_windows.end()) {
        m_windows.erase(it);
    }
    if (m_windows.empty()) {
        shutdown();
    }
}

void Application::run() {
    gtk_main();
}

void Application::shutdown() {
    if (m_running) {
        m_running = false;
        m_windows.clear();
        curl_global_cleanup();
        gtk_main_quit();
    }
}

} // namespace Blueprint::Core
