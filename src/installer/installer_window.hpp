#pragma once

#include <gtk/gtk.h>
#include <memory>
#include <string>
#include "installer/installer_titlebar.hpp"
#include "installer/installer_canvas.hpp"
#include "installer/installer_steps.hpp"

namespace Blueprint::Installer {

class InstallerWindow {
public:
    InstallerWindow();
    ~InstallerWindow();

    void show();
    GtkWindow* getWindow() const { return GTK_WINDOW(m_window); }
    void setStep(int step);

private:
    void initWindow();
    void setupStyling();
    void applyThemeCss(const std::string& bg, const std::string& surface,
                       const std::string& surfaceSubtle, const std::string& border,
                       const std::string& textPrimary, const std::string& textMuted,
                       const std::string& accent, const std::string& btnPrimary,
                       const std::string& btnPrimaryFg, bool isDark);

    static gboolean onWindowDraw(GtkWidget* widget, cairo_t* cr, gpointer user_data);

    GtkWidget* m_window = nullptr;
    GtkWidget* m_rootBox = nullptr;
    GtkWidget* m_splitBox = nullptr;

    std::unique_ptr<InstallerTitlebar> m_titlebar;
    std::unique_ptr<InstallerCanvas> m_canvas;
    std::unique_ptr<InstallerSteps> m_steps;

    GtkCssProvider* m_cssProvider = nullptr;

    // Active colors for window-level cairo background drawing
    float m_bgR = 0.97f, m_bgG = 0.98f, m_bgB = 0.99f;
    float m_borderR = 0.06f, m_borderG = 0.09f, m_borderB = 0.16f, m_borderA = 0.08f;
    bool m_isDark = false;
};

} // namespace Blueprint::Installer
