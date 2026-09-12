#include "installer/installer_window.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace Blueprint::Installer {

static void setupKWinWindowRules() {
    const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
    if (!desktop || std::string(desktop).find("KDE") == std::string::npos) {
        return;
    }

    const char* home = std::getenv("HOME");
    if (!home) return;

    std::filesystem::path kwinrc = std::filesystem::path(home) / ".config" / "kwinrulesrc";
    std::string content;
    if (std::filesystem::exists(kwinrc)) {
        std::ifstream ifs(kwinrc);
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        content = buffer.str();
    }

    bool modified = false;
    if (content.find("lumen-installer") == std::string::npos) {
        std::ofstream ofs(kwinrc, std::ios::app);
        if (ofs.is_open()) {
            ofs << "\n[lumen_installer_rule]\n"
                << "Description=lumen installer borderless\n"
                << "noborder=true\n"
                << "noborderrule=2\n"
                << "wmclass=lumen-installer\n"
                << "wmclassmatch=1\n";
            modified = true;
        }
    }

    if (modified) {
        int rc = system("qdbus6 org.kde.KWin /KWin org.kde.KWin.reconfigure 2>/dev/null || qdbus org.kde.KWin /KWin org.kde.KWin.reconfigure 2>/dev/null");
        (void)rc;
    }
}

static void applyPointerCursorRecursively(GtkWidget* widget) {
    if (!widget) return;

    if (GTK_IS_BUTTON(widget) || GTK_IS_CHECK_BUTTON(widget) || GTK_IS_EVENT_BOX(widget)) {
        gtk_widget_add_events(widget, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
        g_signal_connect(widget, "enter-notify-event", G_CALLBACK(+[](GtkWidget* w, GdkEventCrossing*, gpointer) -> gboolean {
            GdkWindow* win = gtk_widget_get_window(w);
            if (win) {
                GdkCursor* cursor = gdk_cursor_new_from_name(gtk_widget_get_display(w), "pointer");
                if (!cursor) {
                    cursor = gdk_cursor_new_for_display(gtk_widget_get_display(w), GDK_HAND2);
                }
                gdk_window_set_cursor(win, cursor);
                if (cursor) g_object_unref(cursor);
            }
            return FALSE;
        }), nullptr);

        g_signal_connect(widget, "leave-notify-event", G_CALLBACK(+[](GtkWidget* w, GdkEventCrossing*, gpointer) -> gboolean {
            GdkWindow* win = gtk_widget_get_window(w);
            if (win) {
                gdk_window_set_cursor(win, nullptr);
            }
            return FALSE;
        }), nullptr);
    }

    if (GTK_IS_CONTAINER(widget)) {
        gtk_container_forall(GTK_CONTAINER(widget), [](GtkWidget* child, gpointer) {
            applyPointerCursorRecursively(child);
        }, nullptr);
    }
}

InstallerWindow::InstallerWindow() {
    setupKWinWindowRules();
    g_set_prgname("lumen-installer");

    initWindow();
    setupStyling();
}

InstallerWindow::~InstallerWindow() {
    if (m_cssProvider) {
        gtk_style_context_remove_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(m_cssProvider)
        );
        g_object_unref(m_cssProvider);
        m_cssProvider = nullptr;
    }
}

void InstallerWindow::initWindow() {
    m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(m_window), "lumen installer");
    gtk_window_set_wmclass(GTK_WINDOW(m_window), "lumen-installer", "lumen-installer");
    gtk_window_set_default_size(GTK_WINDOW(m_window), 1100, 700);
    gtk_window_set_position(GTK_WINDOW(m_window), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(m_window), FALSE);

    // MANDATORY NUANCE: Frameless window
    gtk_window_set_decorated(GTK_WINDOW(m_window), FALSE);

    // MANDATORY NUANCE: Screen RGBA visual & app_paintable for artifact-free 24px rounded corners
    GdkScreen* screen = gtk_widget_get_screen(m_window);
    GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
    if (visual && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(m_window, visual);
    }
    gtk_widget_set_app_paintable(m_window, TRUE);

    g_signal_connect(m_window, "draw", G_CALLBACK(onWindowDraw), this);
    g_signal_connect(m_window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    // Root layout: vertical box (Titlebar on top, Split Deck below)
    m_rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(m_rootBox), "installer-root");
    gtk_container_add(GTK_CONTAINER(m_window), m_rootBox);

    // Titlebar (height 44 px)
    m_titlebar = std::make_unique<InstallerTitlebar>(GTK_WINDOW(m_window));
    gtk_box_pack_start(GTK_BOX(m_rootBox), m_titlebar->getWidget(), FALSE, FALSE, 0);

    // Split-view box: 40% Left Canvas (440 px), 60% Right Deck (660 px)
    m_splitBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(m_splitBox, TRUE);
    gtk_box_pack_start(GTK_BOX(m_rootBox), m_splitBox, TRUE, TRUE, 0);

    m_canvas = std::make_unique<InstallerCanvas>();
    gtk_box_pack_start(GTK_BOX(m_splitBox), m_canvas->getWidget(), FALSE, FALSE, 0);

    m_steps = std::make_unique<InstallerSteps>(GTK_WINDOW(m_window));
    gtk_box_pack_start(GTK_BOX(m_splitBox), m_steps->getWidget(), TRUE, TRUE, 0);

    // Coordinate step changes
    m_steps->setOnStepChanged([this](int newStep) {
        m_canvas->setStep(newStep);
        applyPointerCursorRecursively(m_steps->getWidget());
    });

    // Coordinate theme switching
    m_steps->setOnThemeChanged([this](const ThemeDefinition& t) {
        applyThemeCss(t.bg, t.surface, t.surfaceSubtle, t.border,
                      t.textPrimary, t.textMuted, t.accent,
                      t.btnPrimary, t.btnPrimaryFg, t.isDark);
        m_canvas->updateThemeColors(t.bg, t.accent, t.isDark);
        m_titlebar->updateTheme(t.isDark, t.textPrimary, t.textMuted);
        gtk_widget_queue_draw(m_window);
    });

    // Coordinate deploy execution
    m_steps->setOnDeploy([this](const InstallConfig& config) {
        std::string err;
        if (InstallExecutor::deploy(config, err)) {
            InstallExecutor::launchLumen(err);
            gtk_window_close(GTK_WINDOW(m_window));
        } else {
            std::cerr << "[Installer] Deployment failed: " << err << std::endl;
        }
    });

    // Coordinate cancel
    m_steps->setOnCancel([this]() {
        gtk_window_close(GTK_WINDOW(m_window));
    });

    applyPointerCursorRecursively(m_window);
}

gboolean InstallerWindow::onWindowDraw(GtkWidget* widget, cairo_t* cr, gpointer user_data) {
    auto* self = static_cast<InstallerWindow*>(user_data);

    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);
    double r = 24.0; // 24px rounded corners

    // 1. Clear complete window surface to full transparency
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // 2. Form 24px rounded rectangle path
    cairo_new_sub_path(cr);
    cairo_arc(cr, w - r, r, r, -G_PI_2, 0);
    cairo_arc(cr, w - r, h - r, r, 0, G_PI_2);
    cairo_arc(cr, r, h - r, r, G_PI_2, G_PI);
    cairo_arc(cr, r, r, r, G_PI, 3 * G_PI_2);
    cairo_close_path(cr);

    // 3. Fill with theme base background
    cairo_set_source_rgba(cr, self->m_bgR, self->m_bgG, self->m_bgB, 1.0);
    cairo_fill_preserve(cr);

    // 4. Subtle 1px outer contour
    cairo_set_source_rgba(cr, self->m_borderR, self->m_borderG, self->m_borderB, self->m_borderA);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    return FALSE; // Allow child widgets to paint
}

void InstallerWindow::setupStyling() {
    m_cssProvider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(m_cssProvider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    // Initial theme matching first theme (Noctiluca)
    if (m_steps && !m_steps->getThemes().empty()) {
        const auto& t = m_steps->getThemes()[0];
        applyThemeCss(t.bg, t.surface, t.surfaceSubtle, t.border,
                      t.textPrimary, t.textMuted, t.accent,
                      t.btnPrimary, t.btnPrimaryFg, t.isDark);
        m_canvas->updateThemeColors(t.bg, t.accent, t.isDark);
        m_titlebar->updateTheme(t.isDark, t.textPrimary, t.textMuted);
    }
}

void InstallerWindow::applyThemeCss(
    const std::string& bg, const std::string& surface,
    const std::string& surfaceSubtle, const std::string& border,
    const std::string& textPrimary, const std::string& textMuted,
    const std::string& accent, const std::string& btnPrimary,
    const std::string& btnPrimaryFg, bool isDark) {

    m_isDark = isDark;

    // Parse bg hex for window-level cairo fill
    if (bg.size() >= 7 && bg[0] == '#') {
        unsigned int val = 0;
        if (sscanf(bg.c_str() + 1, "%x", &val) == 1) {
            m_bgR = ((val >> 16) & 0xFF) / 255.0f;
            m_bgG = ((val >> 8) & 0xFF) / 255.0f;
            m_bgB = (val & 0xFF) / 255.0f;
        }
    }
    if (isDark) {
        m_borderR = 1.0f; m_borderG = 1.0f; m_borderB = 1.0f; m_borderA = 0.12f;
    } else {
        m_borderR = 0.06f; m_borderG = 0.09f; m_borderB = 0.16f; m_borderA = 0.08f;
    }

    std::stringstream ss;
    ss << R"(
    * {
        font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", "Ubuntu", sans-serif;
        outline: none;
    }

    window, .installer-root {
        background-color: )" << bg << R"(;
        border-radius: 24px;
        color: )" << textPrimary << R"(;
    }

    /* Titlebar */
    .installer-titlebar {
        background-color: )" << bg << R"(;
        border-top-left-radius: 24px;
        border-top-right-radius: 24px;
        border-bottom: 1px solid )" << border << R"(;
    }

    .installer-brand-label {
        font-size: 13px;
        font-weight: 700;
        letter-spacing: 0.5px;
        color: )" << textPrimary << R"(;
    }

    .installer-badge {
        font-size: 9px;
        font-weight: 700;
        letter-spacing: 1.2px;
        color: )" << textMuted << R"(;
        background-color: )" << (isDark ? "rgba(255, 255, 255, 0.07)" : "rgba(15, 23, 42, 0.05)") << R"(;
        border: 1px solid )" << (isDark ? "rgba(255, 255, 255, 0.12)" : "rgba(15, 23, 42, 0.08)") << R"(;
        border-radius: 6px;
        padding: 2px 7px;
    }

    .titlebar-btn {
        background: transparent;
        border: none;
        box-shadow: none;
        padding: 0;
        min-width: 30px;
        min-height: 30px;
    }

    /* Left Canvas */
    .installer-canvas {
        background: transparent;
        border-bottom-left-radius: 24px;
    }

    /* Right Deck */
    .installer-deck {
        background-color: )" << surface << R"(;
        border-bottom-right-radius: 24px;
    }

    .deck-title-large {
        font-size: 34px;
        font-weight: 800;
        letter-spacing: -0.5px;
        color: )" << textPrimary << R"(;
    }

    .deck-title {
        font-size: 26px;
        font-weight: 700;
        letter-spacing: -0.3px;
        color: )" << textPrimary << R"(;
    }

    .deck-subtitle {
        font-size: 15px;
        color: )" << textMuted << R"(;
    }

    .deck-section-tag {
        font-size: 11px;
        font-weight: 800;
        letter-spacing: 1.0px;
        color: )" << textMuted << R"(;
    }

    /* Primary & Secondary Action Buttons */
    .deck-btn-primary {
        background-color: )" << btnPrimary << R"(;
        color: )" << btnPrimaryFg << R"(;
        font-size: 15px;
        font-weight: 700;
        border: none;
        border-radius: 26px;
        box-shadow: 0 4px 14px rgba(0, 0, 0, 0.12);
        transition: all 180ms ease;
    }

    .deck-btn-primary:hover {
        background-color: )" << (isDark ? accent : "#334155") << R"(;
        color: )" << (isDark ? "#0E1116" : "#FFFFFF") << R"(;
        box-shadow: 0 6px 20px )" << (isDark ? "rgba(56, 189, 248, 0.25)" : "rgba(15, 23, 42, 0.20)") << R"(;
    }

    .deck-btn-primary:active {
        box-shadow: 0 2px 6px rgba(0, 0, 0, 0.18);
    }

    .deck-btn-secondary {
        background: transparent;
        color: )" << textMuted << R"(;
        font-size: 14px;
        font-weight: 600;
        border: none;
        border-radius: 20px;
        transition: all 150ms ease;
    }

    .deck-btn-secondary:hover {
        background-color: )" << (isDark ? "rgba(255, 255, 255, 0.08)" : "rgba(15, 23, 42, 0.05)") << R"(;
        color: )" << textPrimary << R"(;
    }

    .deck-btn-accent {
        background-color: )" << accent << R"(;
        color: )" << (isDark ? "#0E1116" : "#FFFFFF") << R"(;
        font-size: 13px;
        font-weight: 700;
        border: none;
        border-radius: 20px;
        padding: 0 16px;
        transition: all 150ms ease;
    }

    .deck-btn-accent:hover {
        background-color: )" << (isDark ? "#FFFFFF" : "#1E293B") << R"(;
        color: )" << (isDark ? "#0E1116" : "#FFFFFF") << R"(;
    }

    /* Theme Cards */
    .theme-card {
        background-color: )" << surfaceSubtle << R"(;
        border: 1.5px solid )" << border << R"(;
        border-radius: 14px;
        color: )" << textPrimary << R"(;
        box-shadow: none;
        transition: all 150ms ease;
    }

    .theme-card:hover {
        border-color: )" << accent << R"(;
        background-color: )" << (isDark ? "rgba(255, 255, 255, 0.06)" : "rgba(15, 23, 42, 0.03)") << R"(;
    }

    .theme-card-active {
        border-color: )" << accent << R"(;
        background-color: )" << (isDark ? "rgba(56, 189, 248, 0.14)" : "rgba(56, 189, 248, 0.08)") << R"(;
    }

    .theme-card-name {
        font-size: 14px;
        font-weight: 600;
        color: )" << textPrimary << R"(;
    }

    /* Search Cards */
    .search-card {
        background-color: )" << surfaceSubtle << R"(;
        border: 1.5px solid )" << border << R"(;
        border-radius: 16px;
        box-shadow: none;
        transition: all 150ms ease;
    }

    .search-card:hover {
        border-color: )" << accent << R"(;
    }

    .search-card-active {
        border-color: )" << accent << R"(;
        background-color: )" << (isDark ? "rgba(56, 189, 248, 0.14)" : "rgba(56, 189, 248, 0.08)") << R"(;
    }

    .search-card-title {
        font-size: 15px;
        font-weight: 700;
        color: )" << textPrimary << R"(;
    }

    .search-card-desc {
        font-size: 12px;
        color: )" << textMuted << R"(;
    }

    .search-card-check {
        font-size: 16px;
        font-weight: bold;
        color: )" << accent << R"(;
    }

    /* Segmented Control */
    .segmented-control {
        background-color: )" << surfaceSubtle << R"(;
        border-radius: 24px;
        padding: 4px;
        border: 1px solid )" << border << R"(;
    }

    .segmented-btn {
        background: transparent;
        border: none;
        border-radius: 20px;
        padding: 8px 18px;
        font-size: 13px;
        font-weight: 600;
        color: )" << textMuted << R"(;
        box-shadow: none;
        transition: all 150ms ease;
    }

    .segmented-btn-active {
        background-color: )" << (isDark ? "#21262D" : "#FFFFFF") << R"(;
        color: )" << textPrimary << R"(;
        box-shadow: 0 2px 8px rgba(0, 0, 0, 0.10);
    }

    /* Tor Status Card */
    .tor-status-card {
        background-color: )" << surfaceSubtle << R"(;
        border: 1px solid )" << border << R"(;
        border-radius: 18px;
        padding: 20px;
    }

    .tor-status-text {
        font-size: 14px;
        font-weight: 600;
        color: )" << textPrimary << R"(;
    }

    .tor-status-ok {
        color: #10B981;
        font-weight: 700;
    }

    .tor-status-err {
        color: #F59E0B;
    }

    .tor-warning-text {
        font-size: 13px;
        color: )" << textMuted << R"(;
    }

    /* Checkboxes */
    .deck-checkbox {
        font-size: 14px;
        font-weight: 500;
        color: )" << textPrimary << R"(;
    }

    .deck-error-label {
        font-size: 13px;
        color: #EF4444;
    }

    /* Scrolled window styling */
    scrolledwindow {
        background: transparent;
    }
    )";

    GError* err = nullptr;
    gtk_css_provider_load_from_data(m_cssProvider, ss.str().c_str(), -1, &err);
    if (err) {
        std::cerr << "[Installer] CSS load warning: " << err->message << std::endl;
        g_error_free(err);
    }
}

void InstallerWindow::show() {
    gtk_widget_show_all(m_window);
}

void InstallerWindow::setStep(int step) {
    if (m_steps) m_steps->setStep(step);
    if (m_canvas) m_canvas->setStep(step);
}

} // namespace Blueprint::Installer
