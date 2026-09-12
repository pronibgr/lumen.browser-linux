#include "installer/installer_titlebar.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <unistd.h>

namespace Blueprint::Installer {

namespace fs = std::filesystem;

static std::string locateLogoSvg() {
    char exeBuf[PATH_MAX] = {0};
    ssize_t len = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
    if (len > 0) {
        fs::path exeDir = fs::path(std::string(exeBuf, len)).parent_path();
        if (fs::exists(exeDir / "assets" / "logo.svg")) return (exeDir / "assets" / "logo.svg").string();
        if (fs::exists(exeDir.parent_path() / "assets" / "logo.svg")) return (exeDir.parent_path() / "assets" / "logo.svg").string();
    }
    if (fs::exists("./assets/logo.svg")) return "./assets/logo.svg";
    if (fs::exists("../assets/logo.svg")) return "../assets/logo.svg";

    const char* home = std::getenv("HOME");
    if (home) {
        fs::path shareLogo = fs::path(home) / ".local" / "share" / "lumen" / "assets" / "logo.svg";
        if (fs::exists(shareLogo)) return shareLogo.string();
    }
    return "";
}

static void drawSquircle(cairo_t* cr, double x, double y, double w, double h, double r) {
    if (w < 2.0 * r) r = w / 2.0;
    if (h < 2.0 * r) r = h / 2.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -G_PI / 2.0, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0, G_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, G_PI / 2.0, G_PI);
    cairo_arc(cr, x + r, y + r, r, G_PI, 3.0 * G_PI / 2.0);
    cairo_close_path(cr);
}

InstallerTitlebar::InstallerTitlebar(GtkWindow* parentWindow)
    : m_parentWindow(parentWindow) {
    loadLogo();
    initWidgets();
}

InstallerTitlebar::~InstallerTitlebar() {
    if (m_cachedLogoSurface) {
        cairo_surface_destroy(m_cachedLogoSurface);
        m_cachedLogoSurface = nullptr;
    }
    if (m_logoHandle) {
        g_object_unref(m_logoHandle);
        m_logoHandle = nullptr;
    }
}

void InstallerTitlebar::loadLogo() {
    std::string path = locateLogoSvg();
    if (!path.empty()) {
        GError* err = nullptr;
        m_logoHandle = rsvg_handle_new_from_file(path.c_str(), &err);
        if (err) {
            std::cerr << "[Titlebar] Error loading logo SVG: " << err->message << std::endl;
            g_error_free(err);
            m_logoHandle = nullptr;
        } else if (m_logoHandle) {
            RsvgDimensionData dim;
            rsvg_handle_get_dimensions(m_logoHandle, &dim);
            if (dim.width > 0 && dim.height > 0) {
                m_cachedLogoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 20, 20);
                cairo_t* lcr = cairo_create(m_cachedLogoSurface);
                cairo_scale(lcr, 20.0 / dim.width, 20.0 / dim.height);
                rsvg_handle_render_cairo(m_logoHandle, lcr);
                cairo_destroy(lcr);
            }
        }
    }
}

gboolean InstallerTitlebar::onLogoDraw(GtkWidget* widget, cairo_t* cr, gpointer user_data) {
    (void)widget;
    auto* self = static_cast<InstallerTitlebar*>(user_data);
    if (self->m_cachedLogoSurface) {
        cairo_set_source_surface(cr, self->m_cachedLogoSurface, 0, 0);
        cairo_paint(cr);
        return TRUE;
    }
    // Graceful geometric fallback
    cairo_set_source_rgba(cr, 0.22, 0.74, 0.97, 1.0);
    cairo_arc(cr, 10.0, 10.0, 7.0, 0, 2 * G_PI);
    cairo_fill(cr);
    return TRUE;
}

gboolean InstallerTitlebar::onMinimizeDraw(GtkWidget* widget, cairo_t* cr, gpointer user_data) {
    auto* self = static_cast<InstallerTitlebar*>(user_data);
    gint width = gtk_widget_get_allocated_width(widget);
    gint height = gtk_widget_get_allocated_height(widget);

    GtkStateFlags state = gtk_widget_get_state_flags(widget);
    bool isHover = (state & GTK_STATE_FLAG_PRELIGHT) != 0;
    bool isActive = (state & GTK_STATE_FLAG_ACTIVE) != 0;

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    // Modern squircle background plate on hover/press
    if (isActive) {
        if (self->m_isDark) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.14);
        } else {
            cairo_set_source_rgba(cr, 0.06, 0.09, 0.16, 0.12);
        }
        drawSquircle(cr, 1.0, 1.0, width - 2.0, height - 2.0, 7.5);
        cairo_fill(cr);
    } else if (isHover) {
        if (self->m_isDark) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
        } else {
            cairo_set_source_rgba(cr, 0.06, 0.09, 0.16, 0.06);
        }
        drawSquircle(cr, 1.0, 1.0, width - 2.0, height - 2.0, 7.5);
        cairo_fill(cr);
    }

    // Mathematical horizontal hairline: perfectly centered
    double cx = std::round(width / 2.0);
    double cy = std::round(height / 2.0);
    double halfSpan = 5.0; // 10px width

    if (isHover || isActive) {
        cairo_set_source_rgba(cr, self->m_textPrimaryR, self->m_textPrimaryG, self->m_textPrimaryB, 1.0);
    } else {
        cairo_set_source_rgba(cr, self->m_textMutedR, self->m_textMutedG, self->m_textMutedB, 0.85);
    }

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(cr, 1.4);
    cairo_move_to(cr, cx - halfSpan, cy);
    cairo_line_to(cr, cx + halfSpan, cy);
    cairo_stroke(cr);

    return TRUE; // Handled completely; suppresses default GTK styling
}

gboolean InstallerTitlebar::onCloseDraw(GtkWidget* widget, cairo_t* cr, gpointer user_data) {
    auto* self = static_cast<InstallerTitlebar*>(user_data);
    gint width = gtk_widget_get_allocated_width(widget);
    gint height = gtk_widget_get_allocated_height(widget);

    GtkStateFlags state = gtk_widget_get_state_flags(widget);
    bool isHover = (state & GTK_STATE_FLAG_PRELIGHT) != 0;
    bool isActive = (state & GTK_STATE_FLAG_ACTIVE) != 0;

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    // Modern squircle background plate on hover/press
    if (isActive) {
        // Deep vibrant red
        cairo_set_source_rgba(cr, 0.863, 0.149, 0.149, 1.0); // #DC2626
        drawSquircle(cr, 1.0, 1.0, width - 2.0, height - 2.0, 7.5);
        cairo_fill(cr);
    } else if (isHover) {
        // Bright crimson red
        cairo_set_source_rgba(cr, 0.937, 0.267, 0.267, 1.0); // #EF4444
        drawSquircle(cr, 1.0, 1.0, width - 2.0, height - 2.0, 7.5);
        cairo_fill(cr);
    }

    // Mathematical symmetrical 45° X cross: perfectly centered
    double cx = std::round(width / 2.0);
    double cy = std::round(height / 2.0);
    double d = 4.2; // 8.4px diagonal extent

    if (isHover || isActive) {
        // Pure crisp white over crimson
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    } else {
        cairo_set_source_rgba(cr, self->m_textMutedR, self->m_textMutedG, self->m_textMutedB, 0.85);
    }

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(cr, 1.4);
    cairo_move_to(cr, cx - d, cy - d);
    cairo_line_to(cr, cx + d, cy + d);
    cairo_move_to(cr, cx + d, cy - d);
    cairo_line_to(cr, cx - d, cy + d);
    cairo_stroke(cr);

    return TRUE; // Handled completely; suppresses default GTK styling
}

void InstallerTitlebar::initWidgets() {
    m_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(m_container, -1, 44);
    gtk_style_context_add_class(gtk_widget_get_style_context(m_container), "installer-titlebar");

    // Left section: Logo + Title + Setup Pill Badge
    GtkWidget* leftBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(leftBox, 18);
    gtk_widget_set_valign(leftBox, GTK_ALIGN_CENTER);

    // Vector SVG logo
    m_logoArea = gtk_drawing_area_new();
    gtk_widget_set_size_request(m_logoArea, 20, 20);
    gtk_widget_set_valign(m_logoArea, GTK_ALIGN_CENTER);
    g_signal_connect(m_logoArea, "draw", G_CALLBACK(onLogoDraw), this);
    gtk_box_pack_start(GTK_BOX(leftBox), m_logoArea, FALSE, FALSE, 0);

    // Brand title: strictly lowercase "lumen"
    m_titleLabel = gtk_label_new("lumen");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_titleLabel), "installer-brand-label");
    gtk_widget_set_valign(m_titleLabel, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(leftBox), m_titleLabel, FALSE, FALSE, 0);

    // Modern uppercase capsule badge
    m_badgeLabel = gtk_label_new("SETUP");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_badgeLabel), "installer-badge");
    gtk_widget_set_valign(m_badgeLabel, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(leftBox), m_badgeLabel, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(m_container), leftBox, FALSE, FALSE, 0);

    // Center section: Interactive window drag region
    m_dragArea = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(m_dragArea), FALSE);
    gtk_widget_set_hexpand(m_dragArea, TRUE);

    g_signal_connect(m_dragArea, "button-press-event", G_CALLBACK(+[](GtkWidget* widget, GdkEventButton* event, gpointer user_data) -> gboolean {
        (void)widget;
        auto* self = static_cast<InstallerTitlebar*>(user_data);
        if (event->button == GDK_BUTTON_PRIMARY && event->type == GDK_BUTTON_PRESS) {
            gtk_window_begin_move_drag(self->m_parentWindow,
                                       event->button,
                                       (gint)event->x_root,
                                       (gint)event->y_root,
                                       event->time);
            return TRUE;
        }
        return FALSE;
    }), this);

    gtk_box_pack_start(GTK_BOX(m_container), m_dragArea, TRUE, TRUE, 0);

    // Right section: Precision-engineered Window Controls
    GtkWidget* rightBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_end(rightBox, 16);
    gtk_widget_set_valign(rightBox, GTK_ALIGN_CENTER);

    auto setupButtonCursorAndRedraw = [](GtkWidget* btn) {
        gtk_widget_add_events(btn, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
        g_signal_connect(btn, "enter-notify-event", G_CALLBACK(+[](GtkWidget* w, GdkEventCrossing*, gpointer) -> gboolean {
            GdkWindow* win = gtk_widget_get_window(w);
            if (win) {
                GdkCursor* cursor = gdk_cursor_new_from_name(gtk_widget_get_display(w), "pointer");
                if (!cursor) cursor = gdk_cursor_new_for_display(gtk_widget_get_display(w), GDK_HAND2);
                gdk_window_set_cursor(win, cursor);
            }
            gtk_widget_queue_draw(w);
            return FALSE;
        }), nullptr);
        g_signal_connect(btn, "leave-notify-event", G_CALLBACK(+[](GtkWidget* w, GdkEventCrossing*, gpointer) -> gboolean {
            gtk_widget_queue_draw(w);
            return FALSE;
        }), nullptr);
    };

    // Minimize button
    m_btnMinimize = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(m_btnMinimize), GTK_RELIEF_NONE);
    gtk_widget_set_size_request(m_btnMinimize, 30, 30);
    gtk_widget_set_can_focus(m_btnMinimize, FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(m_btnMinimize), "titlebar-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_btnMinimize), "titlebar-min-btn");
    setupButtonCursorAndRedraw(m_btnMinimize);
    g_signal_connect(m_btnMinimize, "draw", G_CALLBACK(onMinimizeDraw), this);
    g_signal_connect(m_btnMinimize, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        auto* self = static_cast<InstallerTitlebar*>(user_data);
        gtk_window_iconify(self->m_parentWindow);
    }), this);

    // Close button
    m_btnClose = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(m_btnClose), GTK_RELIEF_NONE);
    gtk_widget_set_size_request(m_btnClose, 30, 30);
    gtk_widget_set_can_focus(m_btnClose, FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(m_btnClose), "titlebar-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_btnClose), "titlebar-close-btn");
    setupButtonCursorAndRedraw(m_btnClose);
    g_signal_connect(m_btnClose, "draw", G_CALLBACK(onCloseDraw), this);
    g_signal_connect(m_btnClose, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        auto* self = static_cast<InstallerTitlebar*>(user_data);
        gtk_window_close(self->m_parentWindow);
    }), this);

    gtk_box_pack_start(GTK_BOX(rightBox), m_btnMinimize, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rightBox), m_btnClose, FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(m_container), rightBox, FALSE, FALSE, 0);
}

void InstallerTitlebar::updateTheme(bool isDarkTheme, const std::string& textPrimaryHex, const std::string& textMutedHex) {
    m_isDark = isDarkTheme;
    auto parseHex = [](const std::string& hex, float& r, float& g, float& b) {
        if (hex.size() >= 7 && hex[0] == '#') {
            unsigned int val = 0;
            if (sscanf(hex.c_str() + 1, "%x", &val) == 1) {
                r = ((val >> 16) & 0xFF) / 255.0f;
                g = ((val >> 8) & 0xFF) / 255.0f;
                b = (val & 0xFF) / 255.0f;
            }
        }
    };
    parseHex(textPrimaryHex, m_textPrimaryR, m_textPrimaryG, m_textPrimaryB);
    parseHex(textMutedHex, m_textMutedR, m_textMutedG, m_textMutedB);

    if (m_logoArea) gtk_widget_queue_draw(m_logoArea);
    if (m_btnMinimize) gtk_widget_queue_draw(m_btnMinimize);
    if (m_btnClose) gtk_widget_queue_draw(m_btnClose);
}

} // namespace Blueprint::Installer
