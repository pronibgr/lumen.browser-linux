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
};

struct BrowserSettings {
    AnimSettings anim;
    bool  zenMode     = false;
    std::string searchEngine = "DuckDuckGo";
    bool  aiEnabled   = false;
    std::string aiProvider = "Local Ollama";
    std::string aiModel    = "llama3";
};

class SettingsPanel {
public:
    SettingsPanel();

    void toggle();
    void setVisible(bool v);
    bool isVisible() const;   // true if open or animating open

    BrowserSettings& settings() { return m_settings; }
    const BrowserSettings& settings() const { return m_settings; }

    void update(float dt);
    void draw(cairo_t* cr, double winW, double winH);

    bool handleMouseDown(double mx, double my);
    bool handleMouseMove(double mx, double my);
    bool handleMouseUp  (double mx, double my);
    bool handleKeyPress (uint32_t sym, const char* text);

    bool isInBounds(double mx, double my) const;
    bool wantsRedraw() const;

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

    // Slider drag
    int    m_dragSlider = -1;
    double m_dragSliderX0 = 0;
    double m_dragSliderW  = 0;

    // Cached geometry
    double m_px = 0, m_py = 0, m_pw = 0, m_ph = 0;

    // Drawing helpers
    void drawOverlay(cairo_t* cr, double winW, double winH);
    void drawPanel  (cairo_t* cr);
    void drawSidebar(cairo_t* cr, double x, double y, double w, double h);
    void drawContent(cairo_t* cr, double x, double y, double w, double h, int section);
    void drawAnimationsSection(cairo_t* cr, double x, double y, double w);
    void drawComingSoon(cairo_t* cr, double x, double y, const char* title);

    void drawSlider  (cairo_t* cr, double x, double y, double w,
                      float value, float minV, float maxV,
                      const char* label, bool hovered, int sliderId);
    void drawToggle  (cairo_t* cr, double x, double y, bool on,
                      const char* label, bool hovered, int toggleId);
    void drawLabel   (cairo_t* cr, double x, double y, const char* text,
                      Theme::Color col, float sz = 10.f, bool bold = false);

    void sectionSwitch(int to);
};

} // namespace Blueprint::UI
