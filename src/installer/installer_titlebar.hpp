#pragma once

#include <gtk/gtk.h>
#include <librsvg/rsvg.h>
#include <string>

namespace Blueprint::Installer {

class InstallerTitlebar {
public:
    explicit InstallerTitlebar(GtkWindow* parentWindow);
    ~InstallerTitlebar();

    GtkWidget* getWidget() const { return m_container; }
    void updateTheme(bool isDarkTheme, const std::string& textPrimaryHex, const std::string& textMutedHex);

private:
    void initWidgets();
    void loadLogo();

    static gboolean onLogoDraw(GtkWidget* widget, cairo_t* cr, gpointer user_data);
    static gboolean onMinimizeDraw(GtkWidget* widget, cairo_t* cr, gpointer user_data);
    static gboolean onCloseDraw(GtkWidget* widget, cairo_t* cr, gpointer user_data);

    GtkWindow* m_parentWindow = nullptr;
    GtkWidget* m_container = nullptr;
    GtkWidget* m_logoArea = nullptr;
    GtkWidget* m_titleLabel = nullptr;
    GtkWidget* m_badgeLabel = nullptr;
    GtkWidget* m_dragArea = nullptr;
    GtkWidget* m_btnMinimize = nullptr;
    GtkWidget* m_btnClose = nullptr;

    RsvgHandle* m_logoHandle = nullptr;
    cairo_surface_t* m_cachedLogoSurface = nullptr;

    // Cached theme colors for custom vector drawing
    bool m_isDark = true;
    float m_textPrimaryR = 0.95f, m_textPrimaryG = 0.95f, m_textPrimaryB = 0.96f;
    float m_textMutedR = 0.58f, m_textMutedG = 0.64f, m_textMutedB = 0.72f;
};

} // namespace Blueprint::Installer
