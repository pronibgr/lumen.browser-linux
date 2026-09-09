#pragma once
#include <string>
#include <cstdint>
#include <cairo/cairo.h>
#include "engine/tab_transition.hpp"
#include "theme/colors.hpp"

namespace Blueprint::UI {

struct AnimSettings {
    bool  enabled        = true;
    float tabSlide       = 1.0f;  // tab switch slide speed multiplier
    float tabClose       = 1.0f;  // tab close animation speed
    float settingsOpen   = 1.0f;  // settings panel open/close speed
    float sectionSwitch  = 1.0f;  // settings section switch animation speed
    float reloadSpin     = 1.0f;  // reload button spin speed multiplier
};

struct SearchEngineInfo {
    std::string name;
    std::string urlTemplate;
    bool isCustom = false;
};

struct BrowserSettings {
    AnimSettings anim;
    bool  zenMode     = false;
    std::string searchEngine = "DuckDuckGo";
    std::vector<SearchEngineInfo> searchEngines;
    int activeSearchEngineIndex = 1; // default to DuckDuckGo (index 1)
    bool  aiEnabled   = false;
    std::string aiProvider = "Local Ollama";
    std::string aiModel    = "llama3";

    std::string getActiveSearchTemplate() const {
        if (activeSearchEngineIndex >= 0 && activeSearchEngineIndex < static_cast<int>(searchEngines.size())) {
            return searchEngines[activeSearchEngineIndex].urlTemplate;
        }
        return "https://duckduckgo.com/?q=%s";
    }
};

class SettingsPanel {
public:
    SettingsPanel();

    void toggle();
    void setVisible(bool v);
    bool isVisible() const;   // true if open or animating open

    BrowserSettings& settings() { return m_settings; }
    const BrowserSettings& settings() const { return m_settings; }

    float getSliderVisual(int i) const {
        if (i >= 0 && i < 5) return m_sliderVisual[i];
        return 1.0f;
    }

    void setCreateSearchInput(const std::string& s) { m_createSearchInput = s; }
    const std::string& getCreateSearchInput() const { return m_createSearchInput; }

    bool isCreateSearchModalOpen() const { return m_createSearchModalOpen; }
    void setCreateSearchModalOpen(bool v) {
        m_createSearchModalOpen = v;
        if (v) {
            m_createSearchInput = "https://";
            m_createSearchCursor = static_cast<int>(m_createSearchInput.length());
            m_createSearchSelStart = -1;
            m_createSearchSelEnd = -1;
        }
    }
    bool isDeleteConfirmModalOpen() const { return m_deleteConfirmModalOpen; }
    void triggerDeleteCustomSearchEngine(int index);
    bool isSearchEditMode() const { return m_searchEditMode; }
    void setSearchEditMode(bool v) { m_searchEditMode = v; }

    bool isLumenThresholdModalOpen() const { return m_lumenThresholdModalOpen; }
    void setLumenThresholdModalOpen(bool v) { m_lumenThresholdModalOpen = v; }
    void triggerLumenThresholdModal(Theme::ThemeId targetTheme) {
        m_lumenThresholdModalOpen = true;
        m_pendingLightTheme = targetTheme;
    }
    Theme::ThemeId getPendingLightTheme() const { return m_pendingLightTheme; }

    void update(float dt);
    void draw(cairo_t* cr, double winW, double winH);

    bool handleMouseDown(double mx, double my);
    bool handleMouseMove(double mx, double my);
    bool handleMouseUp  (double mx, double my);
    bool handleKeyPress (uint32_t sym, uint32_t mod, const char* text);

    bool isInBounds(double mx, double my) const;
    bool wantsRedraw() const;

    bool isCreateSearchValid() const;

private:
    BrowserSettings m_settings;

    bool  m_wantOpen    = false;
    float m_openProg    = 0.f;   // 0=closed, 1=fully open
    Engine::Animator m_openAnim;

    // Section navigation
    int   m_section     = 0;
    int   m_prevSection = 0;
    float m_sectionProg = 1.f;   // 1=settled
    Engine::Animator m_sectAnim;
    float m_sectionDir  = 1.f;   // +1 = going right/down, -1 = going left/up

    // Smooth cursor and toggle animations
    float m_sidebarCursorY = 0.f;
    float m_toggleAnim     = 1.f;

    // Hover tracking
    int m_hoveredItem = -1;

    // Slider drag & smooth glide animation
    int    m_dragSlider = -1;
    double m_dragSliderX0 = 0;
    double m_dragSliderW  = 0;
    float  m_sliderVisual[5] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    bool   m_sliderVisualInit = false;
    bool   m_sliderIsDragging = false;

    // search dropdown animation and state
    bool   m_searchDropdownOpen = false;
    float  m_searchDropdownAlpha = 0.0f;
    int    m_hoveredDropdownIdx = -1; // index or 997 for + Create, 998 for Edit/Done, 1000+i for minus
    double m_dropdownX = 0, m_dropdownY = 0, m_dropdownW = 0, m_dropdownH = 0;

    // search engine edit mode state
    bool   m_searchEditMode = false;
    float  m_searchEditModeAlpha = 0.0f;

    // delete search engine confirmation modal dialog state
    bool   m_deleteConfirmModalOpen = false;
    float  m_deleteConfirmModalAlpha = 0.0f;
    int    m_deleteTargetEngineIdx = -1;
    bool   m_hoveredDeleteConfirm = false;
    bool   m_hoveredDeleteCancel = false;

    // create search engine modal dialog state
    bool   m_createSearchModalOpen = false;
    float  m_createSearchModalAlpha = 0.0f;
    std::string m_createSearchInput;
    int    m_createSearchCursor = 0;
    int    m_createSearchSelStart = -1;
    int    m_createSearchSelEnd = -1;
    bool   m_hoveredModalCancel = false;
    bool   m_hoveredModalCreate = false;

    bool hasCreateSearchSelection() const {
        return m_createSearchSelStart >= 0 && m_createSearchSelEnd >= 0 && m_createSearchSelStart != m_createSearchSelEnd;
    }
    void clearCreateSearchSelection() {
        m_createSearchSelStart = -1;
        m_createSearchSelEnd = -1;
    }
    void deleteCreateSearchSelection() {
        if (!hasCreateSearchSelection()) return;
        int s = std::min(m_createSearchSelStart, m_createSearchSelEnd);
        int e = std::max(m_createSearchSelStart, m_createSearchSelEnd);
        s = std::clamp(s, 0, static_cast<int>(m_createSearchInput.length()));
        e = std::clamp(e, 0, static_cast<int>(m_createSearchInput.length()));
        m_createSearchInput.erase(s, e - s);
        m_createSearchCursor = s;
        clearCreateSearchSelection();
    }

    // lumen threshold modal state
    bool   m_lumenThresholdModalOpen = false;
    float  m_lumenThresholdModalAlpha = 0.0f;
    Theme::ThemeId m_pendingLightTheme = Theme::ThemeId::CALCITE;
    bool   m_hoveredLumenCancel = false;
    bool   m_hoveredLumenAccept = false;

    // appearance theme card hover animations
    int    m_hoveredThemeIdx = -1;
    float  m_themeHover[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    double m_appearanceX = 0, m_appearanceY = 0, m_appearanceW = 0;

    // cached geometry
    double m_px = 0, m_py = 0, m_pw = 0, m_ph = 0;
    double m_winW = 0, m_winH = 0;
    double m_searchTriggerX = 0, m_searchTriggerY = 0, m_searchTriggerW = 0, m_searchTriggerH = 0;

    // drawing helpers
    void drawOverlay(cairo_t* cr, double winW, double winH);
    void drawPanel  (cairo_t* cr);
    void drawSidebar(cairo_t* cr, double x, double y, double w, double h);
    void drawContent(cairo_t* cr, double x, double y, double w, double h, int section);
    void drawAnimationsSection(cairo_t* cr, double x, double y, double w);
    void drawAppearanceSection(cairo_t* cr, double x, double y, double w);
    void drawThemePaletteCircle(cairo_t* cr, double cx, double cy, double radius,
                                const Theme::Palette& pal, float hoverProgress, bool isCurrent);
    void drawSearchSection(cairo_t* cr, double x, double y, double w);
    void drawCreateSearchModal(cairo_t* cr, double winW, double winH);
    void drawDeleteConfirmModal(cairo_t* cr, double winW, double winH);
    void drawLumenThresholdModal(cairo_t* cr, double winW, double winH);
    void drawComingSoon(cairo_t* cr, double x, double y, const char* title);

    void drawSlider  (cairo_t* cr, double x, double y, double w,
                      float value, float minV, float maxV,
                      const char* label, bool hovered, int sliderId);
    void drawToggle  (cairo_t* cr, double x, double y, bool on,
                      const char* label, bool hovered, int toggleId);
    void drawLabel   (cairo_t* cr, double x, double y, const char* text,
                      Theme::Color col, float sz = 10.f, bool bold = false);

    void sectionSwitch(int to);
    void commitCustomSearchEngine();
    void deleteCustomSearchEngine(int index);
    void saveCustomEnginesToDb();
    void loadSavedEngines();
    void saveActiveEngine();
};

} // namespace Blueprint::UI
