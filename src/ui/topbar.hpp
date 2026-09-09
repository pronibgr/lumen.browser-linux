#pragma once
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <cstdint>
#include <cairo/cairo.h>
#include "ui/tab_strip.hpp"
#include "ui/omnibox.hpp"
#include "ui/neon_progress.hpp"
#include "ui/settings_panel.hpp"
#include "engine/web_tab.hpp"

namespace Blueprint::UI {

class CompactTopbar {
public:
    CompactTopbar();

    void setTabs(const std::vector<std::shared_ptr<Engine::WebTab>>& tabs, int activeIndex);
    void setActiveUrl(const std::string& url);
    void setLoadProgress(float p);
    void setNavState(bool canBack, bool canFwd, bool canReload = true);
    void setTlsInfo(const Engine::TlsCertificateInfo& info);
    void setSiteData(uint64_t bytes, bool canClear);

    TabStrip&      getTabStrip()    { return m_tabStrip; }
    OmniboxWidget& getOmnibox()     { return m_omnibox; }
    NeonProgress&  getNeonProgress(){ return m_neonProgress; }
    SettingsPanel& getSettings()    { return m_settings; }

    void update(float dt);

    // Draw just the topbar rows (no overlays)
    void draw(cairo_t* cr, double w, double h);

    // Draw overlays on top of the full window (popups, certificate banner, clear data modal, settings)
    void drawOverlays(cairo_t* cr, double winW, double winH);

    bool handleMouseDown(double mx, double my);
    bool handleMouseMove(double mx, double my);
    bool handleMouseUp  (double mx, double my);
    bool handleMouseWheel(double dx);
    bool handleKeyPress(uint32_t sym, uint16_t mod, const char* text);

    void setOnBack          (std::function<void()> cb) { m_onBack = cb; }
    void setOnForward       (std::function<void()> cb) { m_onForward = cb; }
    void setOnReload        (std::function<void()> cb) { m_onReload = cb; }
    void setOnClearData     (std::function<void()> cb) { m_onClearData = cb; }

    bool isSettingsOpen() const { return m_settings.isVisible(); }

    bool isAnyOverlayActive() const;
    bool wantsRedraw() const;

private:
    TabStrip      m_tabStrip;
    OmniboxWidget m_omnibox;
    NeonProgress  m_neonProgress;
    SettingsPanel m_settings;

    bool m_canGoBack    = false;
    bool m_canGoForward = false;
    bool m_canReload    = true;

    // Nav hover: -1=none, 0=Back, 1=Forward, 2=Reload, 3=Settings
    int m_hoveredNav = -1;

    // TLS info
    Engine::TlsCertificateInfo m_tlsInfo;
    bool   m_hoveredLock = false;
    double m_lockX = 0, m_lockY = 0, m_lockW = 26, m_lockH = 26;

    // Real Website Data Monitoring
    uint64_t m_siteDataBytes = 0;
    bool     m_canClearData  = false;

    // Certificate banner popup state
    bool   m_certBannerOpen = false;
    float  m_certBannerAlpha = 0.0f;
    bool   m_hoveredCertTitle = false;
    double m_certTitleHoverTimer = 0.0;
    bool   m_hoveredClearSiteBtn = false;

    // Clear Data modal dialog state
    bool   m_clearModalOpen = false;
    float  m_clearModalAlpha = 0.0f;
    bool   m_hoveredClearConfirm = false;
    bool   m_hoveredClearCancel  = false;
    bool   m_hoveredClearInfo    = false;
    double m_clearInfoHoverTimer = 0.0;

    // Cached layout (row 2)
    double m_cachedW  = 0;
    double m_omniboxX = 0, m_omniboxY = 0, m_omniboxW = 0;
    std::string m_currentUrl;

    std::function<void()> m_onBack, m_onForward, m_onReload, m_onClearData;

    void updateLayout(double w);

    // Row 1: nav buttons + tabs
    void drawRow1(cairo_t* cr, double w);
    // Row 2: omnibox + lock icon
    void drawRow2(cairo_t* cr, double w);

    void drawNavBtn(cairo_t* cr, double cx, double cy, int btnIdx, bool enabled);
    void drawActionBtn(cairo_t* cr, double cx, double cy, int btnIdx, bool active);
    void drawLockIcon(cairo_t* cr, double x, double y);
    void drawCertBanner(cairo_t* cr, double winW, double winH);
    void drawClearDataModal(cairo_t* cr, double winW, double winH);
};

} // namespace Blueprint::UI
