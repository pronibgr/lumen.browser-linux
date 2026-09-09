#include "core/app.hpp"
#include "theme/colors.hpp"
#include "storage/database.hpp"
#include "omnibox/converter.hpp"
#include <iostream>
#include <algorithm>
#include <gdk/gdk.h>

namespace Blueprint::Core {

Application::Application() {}
Application::~Application() { shutdown(); }

bool Application::initialize(int argc, char* argv[]) {
    // Prevent GPU driver DMABUF crash on Wayland/NVIDIA setups
    setenv("WEBKIT_DISABLE_DMABUF_RENDERER", "1", 1);

    if (!gtk_init_check(&argc, &argv)) {
        std::cerr << "[Core] GTK initialization failed\n";
        return false;
    }

    Storage::Database::instance().initialize();

    g_set_prgname("lampa-browser");
    g_set_application_name("lampa browser");

    m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(m_window), m_winW, m_winH);
    gtk_window_set_title(GTK_WINDOW(m_window), "lampa browser");
    gtk_window_set_position(GTK_WINDOW(m_window), GTK_WIN_POS_CENTER);
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    gtk_window_set_wmclass(GTK_WINDOW(m_window), "lampa-browser", "lampa-browser");
    #pragma GCC diagnostic pop
    gtk_window_set_icon_name(GTK_WINDOW(m_window), "lampa-browser");

    // Apply browser logo as window and taskbar icon (supports multi-size pixbufs)
    const char* iconCandidates[] = {
        "assets/logo.svg",
        "../assets/logo.svg",
        "/home/elliot/Проекты/Blueprint Browser/assets/logo.svg"
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
            gtk_window_set_default_icon_list(iconList);
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

    // Dark background for window
    GdkRGBA bgCol{Theme::BG_ABYSS.r, Theme::BG_ABYSS.g, Theme::BG_ABYSS.b, 1.0};
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    gtk_widget_override_background_color(m_window, GTK_STATE_FLAG_NORMAL, &bgCol);
    #pragma GCC diagnostic pop

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

    gtk_overlay_add_overlay(GTK_OVERLAY(m_overlay), m_overlayArea);
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(m_overlay), m_overlayArea, FALSE);
    gtk_widget_hide(m_overlayArea);

    // Window signals
    g_signal_connect(m_window, "key-press-event", G_CALLBACK(onWindowKeyPress), this);
    g_signal_connect(m_window, "scroll-event", G_CALLBACK(onWindowScroll), this);
    g_signal_connect(m_window, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
        static_cast<Application*>(data)->shutdown();
    }), this);

    // Topbar callbacks
    m_topbar.getTabStrip().setCallbacks(
        [this](int oldIdx, int newIdx) { switchTab(oldIdx, newIdx); },
        [this]()         { createTab("lampa://newtab"); },
        [this](int idx)  { closeTab(idx); }
    );

    m_topbar.getOmnibox().setOnNavigate([this](const std::string& url) {
        navigateActiveTab(url);
    });

    // When live exchange rates arrive asynchronously, refresh omnibox suggestions & redraw
    Blueprint::Omnibox::UnitConverter::setOnRatesUpdated([this]() {
        g_idle_add(+[](gpointer data) -> gboolean {
            auto* app = static_cast<Application*>(data);
            if (app->m_topbar.getOmnibox().isFocused()) {
                app->m_topbar.getOmnibox().updateSuggestions();
                gtk_widget_queue_draw(app->m_topbarArea);
                if (app->m_topbar.isAnyOverlayActive()) {
                    gtk_widget_queue_draw(app->m_overlayArea);
                }
            }
            return G_SOURCE_REMOVE;
        }, this);
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

    createTab("lampa://newtab");

    // VSync-synchronized frame tick for native monitor refresh rates (60/120/144/240Hz)
    gtk_widget_add_tick_callback(m_window, +[](GtkWidget*, GdkFrameClock*, gpointer data) -> gboolean {
        auto* app = static_cast<Application*>(data);
        auto now = std::chrono::steady_clock::now();
        float dt = 0.016f;
        if (app->m_hasLastUpdateTime) {
            float realDt = std::chrono::duration_cast<std::chrono::microseconds>(now - app->m_lastUpdateTime).count() / 1000000.f;
            dt = std::clamp(realDt, 0.001f, 0.050f);
        } else {
            app->m_hasLastUpdateTime = true;
        }
        app->m_lastUpdateTime = now;
        app->update(dt);
        return G_SOURCE_CONTINUE;
    }, this, nullptr);

    // Heartbeat fallback timer to ensure animation wakeup when idle
    g_timeout_add(16, +[](gpointer data) -> gboolean {
        auto* app = static_cast<Application*>(data);
        if (app->m_topbar.wantsRedraw()) {
            gtk_widget_queue_draw(app->m_topbarArea);
            if (app->m_topbar.isAnyOverlayActive()) {
                gtk_widget_queue_draw(app->m_overlayArea);
            }
        }
        return G_SOURCE_CONTINUE;
    }, this);

    gtk_widget_show_all(m_window);
    gtk_widget_hide(m_overlayArea); // ensure overlay is hidden initially

    m_running = true;
    return true;
}

bool Application::validActive() const {
    return m_activeIdx >= 0 && m_activeIdx < static_cast<int>(m_tabs.size());
}

void Application::syncTopbar() {
    if (!validActive()) return;
    auto& tab = m_tabs[m_activeIdx];
    m_topbar.setActiveUrl(tab->getUrl());
    m_topbar.setNavState(tab->canGoBack(), tab->canGoForward(), tab->canReload());
    m_topbar.setTlsInfo(tab->getTlsInfo());
    m_topbar.setSiteData(tab->getSiteDataBytes(), tab->canClearData());
    m_topbar.setTabs(m_tabs, m_activeIdx);
    m_topbar.setLoadProgress(tab->getLoadProgress());
}

void Application::createTab(const std::string& url) {
    int newId = m_nextId++;
    auto tab = std::make_shared<Engine::WebTab>(newId, url);
    m_tabs.push_back(tab);

    std::string tabName = "tab_" + std::to_string(newId);
    gtk_stack_add_named(GTK_STACK(m_stack), tab->getWebView(), tabName.c_str());
    gtk_widget_show_all(tab->getWebView());

    // Connect tab signal callbacks
    tab->setCallbacks(
        [this, tab](const std::string&) {
            syncTopbar();
            gtk_widget_queue_draw(m_topbarArea);
        },
        [this, tab](const std::string& u) {
            if (validActive() && m_tabs[m_activeIdx] == tab) {
                syncTopbar();
                Storage::Database::instance().addHistory(u, tab->getTitle());
            }
        },
        [this, tab](float prog) {
            if (validActive() && m_tabs[m_activeIdx] == tab) {
                m_topbar.setLoadProgress(prog);
                gtk_widget_queue_draw(m_topbarArea);
            }
        }
    );

    int newIdx = static_cast<int>(m_tabs.size()) - 1;
    switchTab(m_activeIdx, newIdx);
}

void Application::closeTab(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;

    auto tab = m_tabs[index];
    gtk_container_remove(GTK_CONTAINER(m_stack), tab->getWebView());
    m_tabs.erase(m_tabs.begin() + index);

    if (m_tabs.empty()) {
        createTab("lampa://newtab");
        return;
    }

    m_activeIdx = std::clamp(m_activeIdx, 0, static_cast<int>(m_tabs.size()) - 1);
    std::string tabName = "tab_" + std::to_string(m_tabs[m_activeIdx]->getId());
    gtk_stack_set_visible_child_name(GTK_STACK(m_stack), tabName.c_str());
    syncTopbar();
    gtk_widget_queue_draw(m_topbarArea);
}

void Application::switchTab(int oldIdx, int newIdx) {
    if (newIdx < 0 || newIdx >= static_cast<int>(m_tabs.size())) return;
    if (newIdx == oldIdx) return;
    m_activeIdx = newIdx;

    std::string tabName = "tab_" + std::to_string(m_tabs[newIdx]->getId());

    // Directional Tab Slide:
    // If going right (newIdx > oldIdx), slide LEFT to reveal new tab from the right.
    // If going left (newIdx < oldIdx), slide RIGHT to reveal new tab from the left.
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
}

void Application::navigateActiveTab(const std::string& url) {
    if (!validActive()) return;
    m_tabs[m_activeIdx]->loadUrl(url);
    syncTopbar();
    gtk_widget_queue_draw(m_topbarArea);
}

void Application::zoomIn() {
    if (validActive()) m_tabs[m_activeIdx]->zoomIn();
}

void Application::zoomOut() {
    if (validActive()) m_tabs[m_activeIdx]->zoomOut();
}

void Application::resetZoom() {
    if (validActive()) m_tabs[m_activeIdx]->resetZoom();
}

void Application::handleScrollZoom(double dy) {
    if (validActive()) m_tabs[m_activeIdx]->handleScrollZoom(dy);
}

void Application::clearActiveSiteData() {
    if (!validActive()) return;
    m_tabs[m_activeIdx]->clearWebsiteData([](bool success) {
        std::cout << "[Core] Cleared website data: " << (success ? "OK" : "Failed") << "\n";
    });
}

void Application::update(float dt) {
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
gboolean Application::onTopbarDraw(GtkWidget* widget, cairo_t* cr, gpointer data) {
    auto* self = static_cast<Application*>(data);
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);
    self->m_topbar.draw(cr, w, h);
    return FALSE;
}

gboolean Application::onTopbarMotion(GtkWidget* widget, GdkEventMotion* event, gpointer data) {
    auto* self = static_cast<Application*>(data);
    if (self->m_topbar.handleMouseMove(event->x, event->y)) {
        gtk_widget_queue_draw(widget);
    }
    return TRUE;
}

gboolean Application::onTopbarButtonPress(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    auto* self = static_cast<Application*>(data);
    if (event->button == 1) {
        self->m_topbar.handleMouseDown(event->x, event->y);
        gtk_widget_queue_draw(widget);
        if (self->m_topbar.isAnyOverlayActive()) {
            gtk_widget_queue_draw(self->m_overlayArea);
        }
        return TRUE;
    } else if (event->button == 2) {
        // Middle-click tab close
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

gboolean Application::onTopbarButtonRelease(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    auto* self = static_cast<Application*>(data);
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

gboolean Application::onTopbarScroll(GtkWidget* widget, GdkEventScroll* event, gpointer data) {
    auto* self = static_cast<Application*>(data);
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

gboolean Application::onOverlayDraw(GtkWidget* widget, cairo_t* cr, gpointer data) {
    auto* self = static_cast<Application*>(data);
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);
    self->m_topbar.drawOverlays(cr, w, h);
    return FALSE;
}

gboolean Application::onOverlayMotion(GtkWidget* widget, GdkEventMotion* event, gpointer data) {
    auto* self = static_cast<Application*>(data);
    bool changed = self->m_topbar.handleMouseMove(event->x, event->y);
    if (changed) {
        gtk_widget_queue_draw(widget);
        gtk_widget_queue_draw(self->m_topbarArea);
    }
    return TRUE;
}

gboolean Application::onOverlayButtonPress(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    auto* self = static_cast<Application*>(data);
    if (event->button == 1) {
        self->m_topbar.handleMouseDown(event->x, event->y);
        gtk_widget_queue_draw(widget);
        gtk_widget_queue_draw(self->m_topbarArea);
        return TRUE;
    } else if (event->button == 2) {
        // Middle-click tab close when overlay/omnibox is active
        self->m_topbar.getTabStrip().handleMouseDown(event->x, event->y, 2);
        self->m_topbar.getOmnibox().setFocused(false);
        gtk_widget_queue_draw(widget);
        gtk_widget_queue_draw(self->m_topbarArea);
        return TRUE;
    }
    return FALSE;
}

gboolean Application::onOverlayButtonRelease(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    auto* self = static_cast<Application*>(data);
    if (event->button == 1) {
        if (self->m_topbar.handleMouseUp(event->x, event->y)) {
            gtk_widget_queue_draw(widget);
            gtk_widget_queue_draw(self->m_topbarArea);
            return TRUE;
        }
    }
    return FALSE;
}

gboolean Application::onWindowKeyPress(GtkWidget*, GdkEventKey* event, gpointer data) {
    auto* self = static_cast<Application*>(data);
    bool ctrl = (event->state & GDK_CONTROL_MASK) != 0;
    bool shift = (event->state & GDK_SHIFT_MASK) != 0;

    // 1. First priority: Global browser shortcuts (Tabs, Navigation, Zoom, Zen mode)
    // These should ALWAYS work regardless of focus or active overlays (except when typing in omnibox for Ctrl+A/C/V/X)
    if (ctrl) {
        // Ctrl + N or Ctrl + T: open new tab
        if (event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_N ||
            event->keyval == GDK_KEY_t || event->keyval == GDK_KEY_T) {
            self->createTab("lampa://newtab");
            return TRUE;
        }
        // Ctrl + W: close active tab
        if (event->keyval == GDK_KEY_w || event->keyval == GDK_KEY_W) {
            self->closeTab(self->m_activeIdx);
            return TRUE;
        }
        // Ctrl + L: focus omnibox & select all
        if (event->keyval == GDK_KEY_l || event->keyval == GDK_KEY_L) {
            self->m_topbar.getOmnibox().setFocused(true);
            gtk_widget_queue_draw(self->m_topbarArea);
            return TRUE;
        }
        // Ctrl + Tab: cycle tabs
        if (event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_ISO_Left_Tab) {
            if (!self->m_tabs.empty()) {
                int next = (self->m_activeIdx + 1) % static_cast<int>(self->m_tabs.size());
                self->switchTab(self->m_activeIdx, next);
            }
            return TRUE;
        }
        // Zoom In: Ctrl + Plus / Ctrl + Equal / Ctrl + KP_Add
        if (event->keyval == GDK_KEY_plus || event->keyval == GDK_KEY_equal || event->keyval == GDK_KEY_KP_Add) {
            self->zoomIn();
            return TRUE;
        }
        // Zoom Out: Ctrl + Minus / Ctrl + KP_Subtract
        if (event->keyval == GDK_KEY_minus || event->keyval == GDK_KEY_KP_Subtract) {
            self->zoomOut();
            return TRUE;
        }
        // Reset Zoom: Ctrl + 0 / Ctrl + KP_0
        if (event->keyval == GDK_KEY_0 || event->keyval == GDK_KEY_KP_0) {
            self->resetZoom();
            return TRUE;
        }
    }

    // F11 Zen Mode toggle
    if (event->keyval == GDK_KEY_F11) {
        self->m_zenMode = !self->m_zenMode;
        gtk_widget_set_visible(self->m_topbarArea, !self->m_zenMode);
        return TRUE;
    }

    // 2. Overlay handling (Settings panel, Certificate banner, Clear data modal)
    // Exclude omnibox popup from intercepting normal typing / navigation
    if (self->m_topbar.getSettings().isVisible() || self->m_topbar.isAnyOverlayActive()) {
        if (event->keyval == GDK_KEY_Escape) {
            self->m_topbar.handleKeyPress(event->keyval, event->state, nullptr);
            gtk_widget_queue_draw(self->m_overlayArea);
            gtk_widget_queue_draw(self->m_topbarArea);
            return TRUE;
        }
        // Only route to settings if settings panel is open
        if (self->m_topbar.getSettings().isVisible()) {
            self->m_topbar.handleKeyPress(event->keyval, event->state, event->string);
            gtk_widget_queue_draw(self->m_overlayArea);
            gtk_widget_queue_draw(self->m_topbarArea);
            return TRUE;
        }
    }

    // 3. Omnibox handling when focused (text editing, navigation, selection, suggestions)
    if (self->m_topbar.getOmnibox().isFocused()) {
        if (self->m_topbar.handleKeyPress(event->keyval, event->state, event->string)) {
            gtk_widget_queue_draw(self->m_topbarArea);
            if (self->m_topbar.isAnyOverlayActive()) {
                gtk_widget_queue_draw(self->m_overlayArea);
            }
            return TRUE;
        }
    }

    // Reload: F5 or Ctrl + R (respects canReload, blocked on internal pages)
    if (event->keyval == GDK_KEY_F5 || ((event->keyval == GDK_KEY_r || event->keyval == GDK_KEY_R) && ctrl)) {
        if (self->validActive()) {
            auto tab = self->m_tabs[self->m_activeIdx];
            if (tab && tab->canReload()) {
                tab->reload();
            }
        }
        return TRUE;
    }

    // Shift + T: close current active tab (only when omnibox is not focused)
    if (!self->m_topbar.getOmnibox().isFocused() && shift && !ctrl &&
        (event->keyval == GDK_KEY_T || event->keyval == GDK_KEY_t)) {
        self->closeTab(self->m_activeIdx);
        return TRUE;
    }

    return FALSE;
}

gboolean Application::onWindowScroll(GtkWidget*, GdkEventScroll* event, gpointer data) {
    auto* self = static_cast<Application*>(data);
    if (event->state & GDK_CONTROL_MASK) {
        double dy = 0.0;
        if (event->direction == GDK_SCROLL_UP)   dy = 1.0;
        else if (event->direction == GDK_SCROLL_DOWN) dy = -1.0;
        else if (event->direction == GDK_SCROLL_SMOOTH) dy = -event->delta_y;

        self->handleScrollZoom(dy);
        return TRUE;
    }
    return FALSE;
}

void Application::run() {
    gtk_main();
}

void Application::shutdown() {
    if (m_running) {
        m_running = false;
        gtk_main_quit();
    }
}

} // namespace Blueprint::Core
