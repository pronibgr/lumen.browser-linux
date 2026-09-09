#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "ui/tab_strip.hpp"
#include "theme/colors.hpp"
#include <algorithm>
#include <pango/pangocairo.h>

namespace Blueprint::UI {

namespace {

void sc(cairo_t* cr, const Theme::Color& c, float a = 1.f) {
    cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a * a);
}

void rr(cairo_t* cr, double x, double y, double w, double h, double r) {
    r = std::min(r, std::min(w,h)/2.0);
    cairo_new_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -M_PI/2, 0);
    cairo_arc(cr, x+w-r, y+h-r, r,  0,      M_PI/2);
    cairo_arc(cr, x+r,   y+h-r, r,  M_PI/2, M_PI);
    cairo_arc(cr, x+r,   y+r,   r,  M_PI,   3*M_PI/2);
    cairo_close_path(cr);
}

std::string cleanUtf8(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 0x20 || c == '\t' || c >= 0x80) {
            o += s[i];
        }
    }
    return o;
}

} // namespace

TabStrip::TabStrip() {}

void TabStrip::setTabs(const std::vector<std::shared_ptr<Engine::WebTab>>& tabs, int activeIndex) {
    bool tabCountChanged = (m_tabs.size() != tabs.size());
    m_tabs = tabs;
    m_activeIndex = std::clamp(activeIndex, 0, static_cast<int>(m_tabs.size()) - 1);

    if (m_lastW > 0 && !m_tabs.empty()) {
        double tabW = 0, totalW = 0;
        computeLayout(m_lastW, m_tabs.size(), tabW, totalW);
        m_targetTabW = tabW;
        if (tabCountChanged && m_currentTabW <= 0) {
            m_currentTabW = tabW;
        }
        ensureTabVisible(m_activeIndex);
    }
}

void TabStrip::setCallbacks(TabSwitchCallback sw, TabCreateCallback cr, TabCloseCallback cl) {
    m_onSwitch = sw; m_onCreate = cr; m_onClose = cl;
}

void TabStrip::computeLayout(double width, size_t numTabs, double& tabW, double& totalW) {
    constexpr double ADD_BTN_W = 34.0;
    constexpr double GAP       =  3.0;
    double avail = std::max(50.0, width - ADD_BTN_W);

    if (numTabs == 0) {
        tabW = Theme::TAB_MAX_WIDTH;
        totalW = 0;
        return;
    }

    double natural = (avail - GAP * (numTabs - 1)) / numTabs;
    tabW = std::clamp(natural, 64.0, static_cast<double>(Theme::TAB_MAX_WIDTH));
    totalW = tabW * numTabs + GAP * (numTabs - 1);
}

void TabStrip::ensureTabVisible(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size()) || m_lastW <= 50.0) return;

    constexpr double GAP = 3.0;
    double viewW = m_lastW - 36.0;
    double tabLeft  = index * (m_targetTabW + GAP);
    double tabRight = tabLeft + m_targetTabW;

    if (tabLeft < m_targetScrollOffset) {
        m_targetScrollOffset = tabLeft;
    } else if (tabRight > m_targetScrollOffset + viewW) {
        m_targetScrollOffset = tabRight - viewW;
    }
    m_targetScrollOffset = std::clamp(m_targetScrollOffset, 0.0, m_maxScrollOffset);
}

bool TabStrip::wantsRedraw() const {
    bool hasClosing = false;
    for (const auto& t : m_tabs) {
        if (t && t->isClosing() && t->getCloseProgress() < 1.0f) {
            hasClosing = true;
            break;
        }
    }
    return (std::abs(m_cursorX - m_cursorTargetX) > 0.15)
        || (std::abs(m_cursorW - m_cursorTargetW) > 0.15)
        || (std::abs(m_currentTabW - m_targetTabW) > 0.15)
        || (std::abs(m_scrollOffset - m_targetScrollOffset) > 0.15)
        || hasClosing;
}

void TabStrip::update(float dt) {
    if (m_tabs.empty() || m_lastW <= 0) return;

    for (auto& tab : m_tabs) {
        if (tab && tab->isClosing()) {
            tab->updateClose(dt);
        }
    }

    // Smooth tab resize animation
    float resizeFactor = 1.0f - std::exp(-20.0f * dt);
    m_currentTabW += (m_targetTabW - m_currentTabW) * resizeFactor;

    // Smooth scroll animation
    float scrollFactor = 1.0f - std::exp(-20.0f * dt);
    m_scrollOffset += (m_targetScrollOffset - m_scrollOffset) * scrollFactor;

    // Smooth cursor animation (position and width)
    float cursorFactor = 1.0f - std::exp(-24.0f * dt);
    m_cursorX += (m_cursorTargetX - m_cursorX) * cursorFactor;
    m_cursorW += (m_cursorTargetW - m_cursorW) * cursorFactor;
}

void TabStrip::drawTab(cairo_t* cr, double x, double y, double w, double h,
                       int idx, bool active, bool hovered, bool closeHov,
                       const std::string& title, float closeFade) {
    (void)idx;
    if (closeFade <= 0.01f || w < 20.0) return;

    cairo_push_group(cr);

    // Background
    rr(cr, x, y, w, h, 7);
    if (active) {
        sc(cr, Theme::BG_ACTIVE);
    } else if (hovered) {
        sc(cr, Theme::BG_SUBTLE, 0.9f);
    } else {
        sc(cr, Theme::BG_SUBTLE, 0.45f);
    }
    cairo_fill_preserve(cr);

    // Border
    sc(cr, Theme::BORDER_SOFT, active ? 0.7f : 0.3f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // Title text
    double textX = x + 10;
    double textW = w - 34;
    if (textW > 12) {
        PangoLayout* layout = pango_cairo_create_layout(cr);
        PangoFontDescription* fd = pango_font_description_from_string(active ? "Inter SemiBold 10" : "Inter 10");
        pango_layout_set_font_description(layout, fd);
        pango_font_description_free(fd);
        pango_layout_set_width(layout, static_cast<int>(textW * PANGO_SCALE));
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
        pango_layout_set_single_paragraph_mode(layout, TRUE);

        std::string t = cleanUtf8(title);
        if (t.empty()) t = "New Tab";
        pango_layout_set_text(layout, t.c_str(), -1);

        int th = 0;
        pango_layout_get_pixel_size(layout, nullptr, &th);
        sc(cr, active ? Theme::TEXT_MAIN : Theme::TEXT_MUTED);
        cairo_move_to(cr, textX, y + (h - th) / 2.0);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }

    // Close button
    if ((hovered || active) && w >= 48.0) {
        double cx = x + w - 12;
        double cy = y + h / 2.0;
        if (closeHov) {
            cairo_arc(cr, cx, cy, 7, 0, 2*M_PI);
            sc(cr, Theme::BG_ACTIVE);
            cairo_fill(cr);
            sc(cr, Theme::TEXT_MAIN);
        } else {
            sc(cr, Theme::TEXT_MUTED, 0.6f);
        }
        cairo_set_line_width(cr, 1.3);
        cairo_move_to(cr, cx - 3.5, cy - 3.5); cairo_line_to(cr, cx + 3.5, cy + 3.5);
        cairo_move_to(cr, cx + 3.5, cy - 3.5); cairo_line_to(cr, cx - 3.5, cy + 3.5);
        cairo_stroke(cr);
    }

    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, closeFade);
}

void TabStrip::draw(cairo_t* cr, double x, double y, double width, double height) {
    m_lastX = x; m_lastY = y; m_lastW = width; m_lastH = height;

    if (m_tabs.empty() || width <= 10.0) return;

    size_t numTabs = m_tabs.size();
    double targetW = 0, totalW = 0;
    computeLayout(width, numTabs, targetW, totalW);
    m_targetTabW = targetW;
    if (m_currentTabW <= 0) m_currentTabW = targetW;

    constexpr double GAP = 3.0;

    // Recalculate max scroll based on total width
    double viewWidth = width - 36.0;
    double maxScroll = std::max(0.0, (m_currentTabW + GAP) * numTabs - GAP - viewWidth);
    m_maxScrollOffset = maxScroll;
    m_targetScrollOffset = std::clamp(m_targetScrollOffset, 0.0, m_maxScrollOffset);

    double curTabW = m_currentTabW;
    double tabY = y + (height - Theme::TAB_HEIGHT) / 2.0;
    double curX = x - m_scrollOffset;

    // Target position for the active tab cursor indicator
    m_cursorTargetX = curX + m_activeIndex * (curTabW + GAP);
    m_cursorTargetW = curTabW;

    if (!m_cursorInit) {
        m_cursorX = m_cursorTargetX;
        m_cursorW = m_cursorTargetW;
        m_cursorInit = true;
    }

    // Clip to strip visible area (excluding add button)
    cairo_save(cr);
    cairo_rectangle(cr, x, y, viewWidth, height);
    cairo_clip(cr);

    // Draw active tab indicator (the sliding cursor under the active tab)
    if (m_cursorW >= 20.0) {
        rr(cr, m_cursorX, tabY, m_cursorW, Theme::TAB_HEIGHT, 7);
        sc(cr, Theme::BG_ACTIVE);
        cairo_fill_preserve(cr);
        sc(cr, Theme::ACCENT_CALM, 0.45f);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    // Draw all tabs
    for (size_t i = 0; i < numTabs; ++i) {
        bool active  = (static_cast<int>(i) == m_activeIndex);
        bool hovered = (static_cast<int>(i) == m_hoveredIndex);
        bool cHov    = (static_cast<int>(i) == m_hoveredCloseIndex);

        float closeFade = 1.0f;
        if (m_tabs[i]->isClosing()) {
            closeFade = 1.0f - m_tabs[i]->getCloseProgress();
        }

        double tx = curX + i * (curTabW + GAP);

        // Cull tabs completely outside visible window
        if (tx + curTabW < x - 10.0 || tx > x + viewWidth + 10.0) {
            continue;
        }

        drawTab(cr, tx, tabY, curTabW, Theme::TAB_HEIGHT,
                static_cast<int>(i), active, hovered, cHov,
                m_tabs[i]->getTitle(), closeFade);
    }

    cairo_restore(cr); // restore clip

    // ── Add Tab Button ─────────────────────────────────────────────────────────
    double addX = x + width - 30;
    double addY = y + (height - 24.0) / 2.0;
    rr(cr, addX, addY, 24, 24, 6);
    if (m_hoveredAddButton) {
        sc(cr, Theme::BG_ACTIVE);
        cairo_fill_preserve(cr);
    } else {
        cairo_set_source_rgba(cr, 0, 0, 0, 0);
        cairo_fill_preserve(cr);
    }
    sc(cr, Theme::BORDER_SOFT, m_hoveredAddButton ? 0.8f : 0.35f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    double plusCX = addX + 12, plusCY = addY + 12;
    sc(cr, m_hoveredAddButton ? Theme::TEXT_MAIN : Theme::TEXT_MUTED);
    cairo_set_line_width(cr, 1.6);
    cairo_move_to(cr, plusCX, plusCY - 5); cairo_line_to(cr, plusCX, plusCY + 5);
    cairo_move_to(cr, plusCX - 5, plusCY); cairo_line_to(cr, plusCX + 5, plusCY);
    cairo_stroke(cr);

    // Fade edges if scrollable
    if (m_scrollOffset > 2.0) {
        cairo_pattern_t* pat = cairo_pattern_create_linear(x, 0, x + 18, 0);
        cairo_pattern_add_color_stop_rgba(pat, 0, Theme::BG_SURFACE.r, Theme::BG_SURFACE.g, Theme::BG_SURFACE.b, 0.95);
        cairo_pattern_add_color_stop_rgba(pat, 1, 0, 0, 0, 0);
        cairo_set_source(cr, pat);
        cairo_rectangle(cr, x, y, 18, height);
        cairo_fill(cr);
        cairo_pattern_destroy(pat);
    }
    if (m_scrollOffset < m_maxScrollOffset - 2.0) {
        double fadeX = x + viewWidth - 20;
        cairo_pattern_t* pat = cairo_pattern_create_linear(fadeX, 0, fadeX + 20, 0);
        cairo_pattern_add_color_stop_rgba(pat, 0, 0, 0, 0, 0);
        cairo_pattern_add_color_stop_rgba(pat, 1, Theme::BG_SURFACE.r, Theme::BG_SURFACE.g, Theme::BG_SURFACE.b, 0.95);
        cairo_set_source(cr, pat);
        cairo_rectangle(cr, fadeX, y, 20, height);
        cairo_fill(cr);
        cairo_pattern_destroy(pat);
    }
}

bool TabStrip::handleMouseMove(double mx, double my) {
    if (m_tabs.empty() || m_lastW <= 0) return false;

    size_t n = m_tabs.size();
    constexpr double GAP = 3.0;
    double tabY = m_lastY + (m_lastH - Theme::TAB_HEIGHT) / 2.0;
    double viewWidth = m_lastW - 36.0;

    int oldHov = m_hoveredIndex, oldCHov = m_hoveredCloseIndex;
    bool oldAdd = m_hoveredAddButton;
    m_hoveredIndex = -1;
    m_hoveredCloseIndex = -1;
    m_hoveredAddButton = false;

    // Check add button first
    double addX = m_lastX + m_lastW - 30;
    double addY = m_lastY + (m_lastH - 24.0) / 2.0;
    if (mx >= addX && mx <= addX + 24 && my >= addY && my <= addY + 24) {
        m_hoveredAddButton = true;
    }

    // Strict bounds check: only test tabs if within visible strip bounds!
    if (mx >= m_lastX && mx <= m_lastX + viewWidth && my >= m_lastY && my <= m_lastY + m_lastH) {
        double curX = m_lastX - m_scrollOffset;
        for (size_t i = 0; i < n; ++i) {
            double tx = curX + i * (m_currentTabW + GAP);
            if (mx >= tx && mx <= tx + m_currentTabW && my >= tabY && my <= tabY + Theme::TAB_HEIGHT) {
                m_hoveredIndex = static_cast<int>(i);
                double cx = tx + m_currentTabW - 12, cy = tabY + Theme::TAB_HEIGHT / 2.0;
                if (std::hypot(mx - cx, my - cy) <= 8) {
                    m_hoveredCloseIndex = static_cast<int>(i);
                }
                break;
            }
        }
    }

    return (oldHov != m_hoveredIndex || oldCHov != m_hoveredCloseIndex || oldAdd != m_hoveredAddButton);
}

bool TabStrip::handleMouseDown(double mx, double my, int button) {
    if (m_tabs.empty()) return false;
    // Strict vertical bounds check: Tab strip only occupies Row 1
    if (my < 0.0 || my > Theme::ROW1_HEIGHT) return false;

    double viewWidth = m_lastW - 36.0;

    if (m_hoveredAddButton) {
        if (m_onCreate) m_onCreate();
        return true;
    }

    // Only process tab clicks if within visible strip area
    if (mx >= m_lastX && mx <= m_lastX + viewWidth) {
        // Detect tab under cursor directly if not cached
        int targetIdx = m_hoveredIndex;
        if (targetIdx < 0) {
            size_t n = m_tabs.size();
            constexpr double GAP = 3.0;
            double curX = m_lastX - m_scrollOffset;
            for (size_t i = 0; i < n; ++i) {
                double tx = curX + i * (m_currentTabW + GAP);
                if (mx >= tx && mx <= tx + m_currentTabW) {
                    targetIdx = static_cast<int>(i);
                    break;
                }
            }
        }

        // Middle click: close tab under cursor
        if (button == 2 && targetIdx >= 0) {
            if (m_onClose) m_onClose(targetIdx);
            return true;
        }

        if (m_hoveredCloseIndex >= 0) {
            if (m_onClose) m_onClose(m_hoveredCloseIndex);
            return true;
        }

        if (targetIdx >= 0 && targetIdx != m_activeIndex) {
            if (m_onSwitch) m_onSwitch(m_activeIndex, targetIdx);
            return true;
        }
    }

    return false;
}

bool TabStrip::handleScroll(double dx) {
    m_targetScrollOffset = std::clamp(m_targetScrollOffset + dx * 40.0, 0.0, m_maxScrollOffset);
    return true;
}

} // namespace Blueprint::UI
