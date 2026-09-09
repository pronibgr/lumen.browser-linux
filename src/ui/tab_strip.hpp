#pragma once
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <cairo/cairo.h>
#include "engine/web_tab.hpp"

namespace Blueprint::UI {

using TabSwitchCallback = std::function<void(int oldIdx, int newIdx)>;
using TabCreateCallback = std::function<void()>;
using TabCloseCallback  = std::function<void(int idx)>;

class TabStrip {
public:
    TabStrip();

    void setTabs(const std::vector<std::shared_ptr<Engine::WebTab>>& tabs, int activeIndex);
    void setCallbacks(TabSwitchCallback onSwitch, TabCreateCallback onCreate, TabCloseCallback onClose);

    void draw(cairo_t* cr, double x, double y, double width, double height);
    void update(float dt);

    bool handleMouseMove (double mx, double my);
    bool handleMouseDown (double mx, double my, int button = 1); // button: 1=left 2=middle
    bool handleScroll    (double dx);

    bool wantsRedraw() const;

    void ensureTabVisible(int index);

    double getLastTabWidth() const { return m_currentTabW; }

private:
    std::vector<std::shared_ptr<Engine::WebTab>> m_tabs;
    int m_activeIndex = 0;

    TabSwitchCallback m_onSwitch;
    TabCreateCallback m_onCreate;
    TabCloseCallback  m_onClose;

    // Hover state
    int  m_hoveredIndex      = -1;
    int  m_hoveredCloseIndex = -1;
    bool m_hoveredAddButton  = false;

    // Scroll (when too many tabs)
    double m_scrollOffset       = 0.0;
    double m_targetScrollOffset = 0.0;
    double m_maxScrollOffset    = 0.0;

    // Layout cache & animated tab width
    double m_lastX = 0, m_lastY = 0, m_lastW = 0, m_lastH = 0;
    double m_currentTabW = 180.0;
    double m_targetTabW  = 180.0;

    // Active tab cursor animation (x position and width)
    double m_cursorX       = 0.0;
    double m_cursorTargetX = 0.0;
    double m_cursorW       = 180.0;
    double m_cursorTargetW = 180.0;
    bool   m_cursorInit    = false;

    void drawTab(cairo_t* cr, double x, double y, double w, double h,
                 int idx, bool active, bool hovered, bool closeHov,
                 const std::string& title, float closeFade);
    void computeLayout(double width, size_t numTabs, double& tabW, double& totalW);
};

} // namespace Blueprint::UI
