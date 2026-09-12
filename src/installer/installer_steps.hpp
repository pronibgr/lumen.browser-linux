#pragma once

#include <gtk/gtk.h>
#include <functional>
#include <string>
#include <vector>
#include "installer/install_executor.hpp"

namespace Blueprint::Installer {

struct ThemeDefinition {
    std::string id;
    std::string name;
    bool isDark;
    std::string colors[3]; // Preview circle colors
    uint32_t previewRgb[3] = {0, 0, 0};
    std::string bg;
    std::string surface;
    std::string surfaceSubtle;
    std::string border;
    std::string textPrimary;
    std::string textMuted;
    std::string accent;
    std::string btnPrimary;
    std::string btnPrimaryFg;
};

struct SearchEngineDefinition {
    std::string name;
    std::string description;
    std::string urlTemplate;
    std::string iconFile;
};

struct SearchIconCache {
    cairo_surface_t* colorSurface = nullptr;
    cairo_surface_t* grayscaleSurface = nullptr;
};

class InstallerSteps {
public:
    explicit InstallerSteps(GtkWindow* parentWindow);
    ~InstallerSteps();

    GtkWidget* getWidget() const { return m_container; }

    void setStep(int step);
    int getCurrentStep() const { return m_currentStep; }

    const InstallConfig& getConfig() const { return m_config; }

    using StepChangedCallback = std::function<void(int newStep)>;
    using ThemeChangedCallback = std::function<void(const ThemeDefinition& theme)>;
    using CancelCallback = std::function<void()>;
    using DeployCallback = std::function<void(const InstallConfig& config)>;

    void setOnStepChanged(StepChangedCallback cb) { m_onStepChanged = cb; }
    void setOnThemeChanged(ThemeChangedCallback cb) { m_onThemeChanged = cb; }
    void setOnCancel(CancelCallback cb) { m_onCancel = cb; }
    void setOnDeploy(DeployCallback cb) { m_onDeploy = cb; }

    const std::vector<ThemeDefinition>& getThemes() const { return m_themes; }

private:
    void initThemes();
    void initSearchEngines();
    void precacheSearchIcons();

    void buildStep0Welcome();
    void buildStep1Appearance();
    void buildStep2Search();
    void buildStep3Tor();
    void buildStep4Deploy();

    void checkTorSocket(int port);
    void runTorProvisioning();

    GtkWindow* m_parentWindow = nullptr;
    GtkWidget* m_container = nullptr;
    GtkWidget* m_stack = nullptr;

    int m_currentStep = 0;
    InstallConfig m_config;

    std::vector<ThemeDefinition> m_themes;
    std::vector<SearchEngineDefinition> m_searchEngines;
    std::vector<SearchIconCache> m_searchIconCaches;
    int m_selectedThemeIndex = 0;
    int m_selectedSearchIndex = 0;

    // Step 1 theme card widgets
    std::vector<GtkWidget*> m_themeCards;

    // Step 2 search card widgets
    std::vector<GtkWidget*> m_searchCards;
    std::vector<GtkWidget*> m_searchCheckmarks;
    std::vector<GtkWidget*> m_searchIconAreas;

    // Step 3 Tor widgets
    GtkWidget* m_btnTorDaemon = nullptr;
    GtkWidget* m_btnTorBundle = nullptr;
    GtkWidget* m_lblTorStatus = nullptr;
    GtkWidget* m_boxTorWarning = nullptr;
    GtkWidget* m_lblTorWarning = nullptr;
    GtkWidget* m_btnTorAutoConfig = nullptr;
    GtkWidget* m_spinnerTor = nullptr;
    bool m_torChecking = false;

    // Step 4 widgets
    GtkWidget* m_chkDesktop = nullptr;
    GtkWidget* m_chkDefaultBrowser = nullptr;
    GtkWidget* m_btnDeploy = nullptr;
    GtkWidget* m_lblDeployError = nullptr;

    StepChangedCallback m_onStepChanged;
    ThemeChangedCallback m_onThemeChanged;
    CancelCallback m_onCancel;
    DeployCallback m_onDeploy;
};

} // namespace Blueprint::Installer
