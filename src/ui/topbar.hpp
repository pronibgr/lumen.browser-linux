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
    void setEphemeral(bool ephem) {
        m_isEphemeral = ephem;
        m_omnibox.setEphemeral(ephem);
    }
    bool isEphemeral() const { return m_isEphemeral; }

    TabStrip&      getTabStrip()    { return m_tabStrip; }
    OmniboxWidget& getOmnibox()     { return m_omnibox; }
    NeonProgress&  getNeonProgress(){ return m_neonProgress; }
    SettingsPanel& getSettings()    { return m_settings; }

    void update(float dt);

    // just topbar rows without full-window overlays
    void draw(cairo_t* cr, double w, double h);

    // full-window popups and overlays (cert banner, modal, settings)
    void drawOverlays(cairo_t* cr, double winW, double winH);

    bool handleMouseDown(double mx, double my);
    bool handleMouseMove(double mx, double my);
    bool handleMouseUp  (double mx, double my);
    bool handleMouseWheel(double dx);
    bool handleKeyPress(uint32_t sym, uint32_t mod, const char* text);

    void setOnBack          (std::function<void()> cb) { m_onBack = cb; }
    void setOnForward       (std::function<void()> cb) { m_onForward = cb; }
    void setOnReload        (std::function<void()> cb) { m_onReload = cb; }
    void setOnClearData     (std::function<void()> cb) { m_onClearData = cb; }
    void setOnRefreshSiteData(std::function<void()> cb) { m_onRefreshSiteData = cb; }
    void setOnNewTab        (std::function<void()> cb) { m_onNewTab = cb; }
    void setOnMinimize      (std::function<void()> cb) { m_onMinimize = cb; }
    void setOnMaximizeToggle(std::function<void()> cb) { m_onMaximizeToggle = cb; }
    void setOnCloseWindow   (std::function<void()> cb) { m_onCloseWindow = cb; }

    void setMaximized(bool max) { m_isMaximized = max; }
    bool isMaximized() const    { return m_isMaximized; }

    bool isSettingsOpen() const { return m_settings.isVisible(); }

    bool isAnyOverlayActive() const;
    bool wantsRedraw() const;

private:
    bool          m_isEphemeral = false;
    TabStrip      m_tabStrip;
    OmniboxWidget m_omnibox;
    NeonProgress  m_neonProgress;
    SettingsPanel m_settings;

    bool m_canGoBack    = false;
    bool m_canGoForward = false;
    bool m_canReload    = true;

    // nav button hover index: -1=none, 0=back, 1=forward, 2=reload
    int m_hoveredNav = -1;

    // window button hover index: -1=none, 0=minimize, 1=maximize, 2=close
    int  m_hoveredWinBtn = -1;
    bool m_isMaximized   = false;

    // row 2 action button hover index: -1=none, 0=new tab, 1=settings
    int  m_hoveredRow2Btn = -1;

    // reload vortex rotation physics
    float m_reloadSpinAngle = 0.0f;
    float m_reloadSpinSpeed = 0.0f;

    // tls info and lock widget bounds
    Engine::TlsCertificateInfo m_tlsInfo;
    bool   m_hoveredLock = false;
    double m_lockX = 0, m_lockY = 0, m_lockW = 26, m_lockH = 26;

    // site storage usage in bytes
    uint64_t m_siteDataBytes = 0;
    bool     m_canClearData  = false;

    // cert banner popup animation and hover states
    bool   m_certBannerOpen = false;
    float  m_certBannerAlpha = 0.0f;
    bool   m_hoveredCertTitle = false;
    double m_certTitleHoverTimer = 0.0;
    bool   m_hoveredClearSiteBtn = false;

    // clear site data modal confirmation dialog
    bool   m_clearModalOpen = false;
    float  m_clearModalAlpha = 0.0f;
    bool   m_hoveredClearConfirm = false;
    bool   m_hoveredClearCancel  = false;
    bool   m_hoveredClearInfo    = false;
    double m_clearInfoHoverTimer = 0.0;

    // cached layout measurements (row 2)
    double m_cachedW  = 0;
    double m_omniboxX = 0, m_omniboxY = 0, m_omniboxW = 0;
    std::string m_currentUrl;

    std::function<void()> m_onBack, m_onForward, m_onReload, m_onClearData, m_onRefreshSiteData;
    std::function<void()> m_onNewTab, m_onMinimize, m_onMaximizeToggle, m_onCloseWindow;

    void updateLayout(double w);

    // row 1: nav buttons + tabs + window controls
    void drawRow1(cairo_t* cr, double w);
    // row 2: omnibox + lock icon + new tab (+) & settings
    void drawRow2(cairo_t* cr, double w);

    void drawNavBtn(cairo_t* cr, double cx, double cy, int btnIdx, bool enabled);
    void drawWindowBtn(cairo_t* cr, double cx, double cy, int btnIdx);
    void drawRow2Btn(cairo_t* cr, double cx, double cy, int btnIdx);
    void drawLockIcon(cairo_t* cr, double x, double y);
    void drawCertBanner(cairo_t* cr, double winW, double winH);
    void drawClearDataModal(cairo_t* cr, double winW, double winH);
};

} // namespace Blueprint::UI
