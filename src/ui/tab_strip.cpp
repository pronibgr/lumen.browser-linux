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
    m_hoveredIndex = -1;
    m_hoveredCloseIndex = -1;

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

void TabStrip::setCallbacks(TabSwitchCallback sw, TabCloseCallback cl) {
    m_onSwitch = sw; m_onClose = cl;
}

void TabStrip::computeLayout(double width, size_t numTabs, double& tabW, double& totalW) {
    constexpr double GAP = 3.0;
    double avail = std::max(50.0, width);

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
    double viewW = m_lastW;
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

    // smooth tab resize animation (exponential decay)
    float resizeFactor = 1.0f - std::exp(-20.0f * dt);
    m_currentTabW += (m_targetTabW - m_currentTabW) * resizeFactor;

    // smooth horizontal scroll
    float scrollFactor = 1.0f - std::exp(-20.0f * dt);
    m_scrollOffset += (m_targetScrollOffset - m_scrollOffset) * scrollFactor;

    // smooth indicator cursor gliding between tabs
    float cursorFactor = 1.0f - std::exp(-24.0f * dt);
    m_cursorX += (m_cursorTargetX - m_cursorX) * cursorFactor;
    m_cursorW += (m_cursorTargetW - m_cursorW) * cursorFactor;

    m_animTime += dt;
}

void TabStrip::drawTab(cairo_t* cr, double x, double y, double w, double h,
                       int idx, bool active, bool hovered, bool closeHov,
                       const std::string& title, float closeFade,
                       bool isOnion, bool isLoading) {
    (void)idx;
    if (closeFade <= 0.01f || w < 20.0) return;

    cairo_push_group(cr);

    // 1. Tab body pill
    rr(cr, x, y, w, h, 7);
    if (isOnion) {
        if (active) {
            cairo_set_source_rgb(cr, 0.035, 0.063, 0.102); // #09101A (cold deep obsidian)
        } else if (hovered) {
            cairo_set_source_rgb(cr, 0.063, 0.102, 0.157); // #101A28
        } else {
            cairo_set_source_rgb(cr, 0.047, 0.078, 0.125); // #0C1420
        }
    } else {
        if (active) {
            sc(cr, Theme::BG_ACTIVE);
        } else if (hovered) {
            sc(cr, Theme::BG_SUBTLE, 0.9f);
        } else {
            sc(cr, Theme::BG_SUBTLE, 0.45f);
        }
    }
    cairo_fill_preserve(cr);

    // 2. Subtle outline border
    if (isOnion) {
        if (active) {
            cairo_set_source_rgba(cr, 0.35, 0.65, 1.0, 0.70); // #58a6ff
            cairo_set_line_width(cr, 1.0);
        } else {
            cairo_set_source_rgba(cr, 0.48, 0.64, 0.97, 0.28);
            cairo_set_line_width(cr, 1.0);
        }
    } else {
        sc(cr, Theme::BORDER_SOFT, active ? 0.7f : 0.3f);
        cairo_set_line_width(cr, 1.0);
    }
    cairo_stroke(cr);

    // 3. Tor 3-segment circuit rail at the top (if onion)
    if (isOnion && w >= 36.0) {
        double railX = x + 6.0;
        double railW = w - 12.0;
        double segW = (railW - 5.0) / 3.0; // 3 segments with 2.5 gap
        double railY = y + 1.0;
        double segH = 2.0;

        float pulse = std::fmod(m_animTime, 1.2f) / 1.2f;

        for (int s = 0; s < 3; ++s) {
            double sx = railX + s * (segW + 2.5);
            rr(cr, sx, railY, segW, segH, 1.0);

            if (isLoading) {
                // Traveling circuit pulse animation
                float segPhase = s * 0.2f;
                float diff = std::fabs(pulse - segPhase);
                if (diff > 0.5f) diff = 1.0f - diff;
                float intensity = std::clamp(1.0f - diff / 0.25f, 0.0f, 1.0f);
                cairo_set_source_rgba(cr, 0.48, 0.64, 0.97, 0.20 + 0.80 * intensity);
            } else if (active) {
                cairo_set_source_rgba(cr, 0.48, 0.64, 0.97, 0.85);
            } else {
                cairo_set_source_rgba(cr, 0.48, 0.64, 0.97, 0.25);
            }
            cairo_fill(cr);
        }
    }

    // 4. Onion Glyph icon if onion tab
    double iconOffset = 0.0;
    if (isOnion && w >= 44.0) {
        double gx = x + 10.0;
        double gy = y + (h - 12.0) / 2.0;
        double cx = gx + 6.0;
        double cy = gy + 6.8;

        cairo_save(cr);
        cairo_set_source_rgba(cr, 0.48, 0.64, 0.97, active ? 1.0 : 0.75); // #7aa2f7
        cairo_set_line_width(cr, 1.1);

        // Outer contour
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, 4.4, 0, 2 * M_PI);
        cairo_stroke(cr);

        // Inner dashed ring
        const double dashes[] = { 2.0, 1.2 };
        cairo_set_dash(cr, dashes, 2, 0);
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy + 0.2, 2.6, 0, 2 * M_PI);
        cairo_stroke(cr);
        cairo_set_dash(cr, nullptr, 0, 0);

        // Center core
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy + 0.4, 0.9, 0, 2 * M_PI);
        cairo_fill(cr);

        // Neck sprout
        cairo_new_path(cr);
        cairo_move_to(cr, cx, cy - 4.4); cairo_line_to(cr, cx, cy - 6.2);
        cairo_move_to(cr, cx - 1.5, cy - 5.5); cairo_line_to(cr, cx, cy - 6.2); cairo_line_to(cr, cx + 1.5, cy - 5.5);
        cairo_stroke(cr);

        cairo_restore(cr);

        iconOffset = 16.0;
    }

    // 5. Tab title text with ellipsize
    double textX = x + 10.0 + iconOffset;
    double textW = w - 34.0 - iconOffset;
    if (textW > 12) {
        PangoLayout* layout = pango_cairo_create_layout(cr);
        std::string fontName;
        if (isOnion) {
            fontName = active ? "JetBrains Mono SemiBold 9.5" : "JetBrains Mono 9.5";
        } else {
            fontName = active ? "Inter SemiBold 10" : "Inter 10";
        }
        PangoFontDescription* fd = pango_font_description_from_string(fontName.c_str());
        pango_layout_set_font_description(layout, fd);
        pango_font_description_free(fd);
        pango_layout_set_width(layout, static_cast<int>(textW * PANGO_SCALE));
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
        pango_layout_set_single_paragraph_mode(layout, TRUE);

        std::string t = cleanUtf8(title);
        if (t.empty()) t = isOnion ? "Onion Service" : "New Tab";
        pango_layout_set_text(layout, t.c_str(), -1);

        int th = 0;
        pango_layout_get_pixel_size(layout, nullptr, &th);
        if (isOnion) {
            cairo_set_source_rgb(cr, 0.86, 0.92, 0.99); // #dbeafe
        } else {
            sc(cr, active ? Theme::TEXT_MAIN : Theme::TEXT_MUTED);
        }
        cairo_move_to(cr, textX, y + (h - th) / 2.0);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }

    // 6. Close button
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

    // recompute max scroll based on total tabs width
    double viewWidth = width;
    double maxScroll = std::max(0.0, (m_currentTabW + GAP) * numTabs - GAP - viewWidth);
    m_maxScrollOffset = maxScroll;
    m_targetScrollOffset = std::clamp(m_targetScrollOffset, 0.0, m_maxScrollOffset);

    double curTabW = m_currentTabW;
    double tabY = y + (height - Theme::TAB_HEIGHT) / 2.0;
    double curX = x - m_scrollOffset;

    // target x pos for active tab cursor highlight
    m_cursorTargetX = curX + m_activeIndex * (curTabW + GAP);
    m_cursorTargetW = curTabW;

    if (!m_cursorInit) {
        m_cursorX = m_cursorTargetX;
        m_cursorW = m_cursorTargetW;
        m_cursorInit = true;
    }

    // clip tabs to strip area
    cairo_save(cr);
    cairo_rectangle(cr, x, y, viewWidth, height);
    cairo_clip(cr);

    // sliding active tab indicator
    if (m_cursorW >= 20.0) {
        rr(cr, m_cursorX, tabY, m_cursorW, Theme::TAB_HEIGHT, 7);
        sc(cr, Theme::BG_ACTIVE);
        cairo_fill_preserve(cr);
        sc(cr, Theme::ACCENT_CALM, 0.45f);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    // draw each visible tab
    for (size_t i = 0; i < numTabs; ++i) {
        bool active  = (static_cast<int>(i) == m_activeIndex);
        bool hovered = (static_cast<int>(i) == m_hoveredIndex);
        bool cHov    = (static_cast<int>(i) == m_hoveredCloseIndex);

        float closeFade = 1.0f;
        if (m_tabs[i]->isClosing()) {
            closeFade = 1.0f - m_tabs[i]->getCloseProgress();
        }

        double tx = curX + i * (curTabW + GAP);

        // cull tabs completely outside visible window
        if (tx + curTabW < x - 10.0 || tx > x + viewWidth + 10.0) {
            continue;
        }

        bool isOnion = m_tabs[i] && m_tabs[i]->isOnion();
        bool isLoading = m_tabs[i] && m_tabs[i]->isLoading();

        drawTab(cr, tx, tabY, curTabW, Theme::TAB_HEIGHT,
                static_cast<int>(i), active, hovered, cHov,
                m_tabs[i]->getTitle(), closeFade,
                isOnion, isLoading);
    }

    cairo_restore(cr); // restore clip

    // fade out edges if scrollable
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
    double viewWidth = m_lastW;

    int oldHov = m_hoveredIndex, oldCHov = m_hoveredCloseIndex;
    m_hoveredIndex = -1;
    m_hoveredCloseIndex = -1;

    // only check tab bounds if within the visible strip
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

    return (oldHov != m_hoveredIndex || oldCHov != m_hoveredCloseIndex);
}

bool TabStrip::handleMouseDown(double mx, double my, int button) {
    if (m_tabs.empty() || m_lastW <= 0) return false;
    // tab strip is strictly in row 1
    if (my < 0.0 || my > Theme::ROW1_HEIGHT) return false;

    double viewWidth = m_lastW;
    if (mx < m_lastX || mx > m_lastX + viewWidth) return false;

    size_t n = m_tabs.size();
    constexpr double GAP = 3.0;
    double tabY = m_lastY + (m_lastH - Theme::TAB_HEIGHT) / 2.0;
    double curX = m_lastX - m_scrollOffset;

    int targetIdx = -1;
    bool clickedClose = false;

    for (size_t i = 0; i < n; ++i) {
        double tx = curX + i * (m_currentTabW + GAP);
        if (mx >= tx && mx <= tx + m_currentTabW && my >= tabY && my <= tabY + Theme::TAB_HEIGHT) {
            targetIdx = static_cast<int>(i);
            double cx = tx + m_currentTabW - 12.0;
            double cy = tabY + Theme::TAB_HEIGHT / 2.0;
            if (std::hypot(mx - cx, my - cy) <= 9.0) {
                clickedClose = true;
            }
            break;
        }
    }

    if (targetIdx < 0 || targetIdx >= static_cast<int>(n)) return false;

    // Reset hover states so subsequent rapid clicks without mouse move don't use stale indices
    m_hoveredIndex = -1;
    m_hoveredCloseIndex = -1;

    // Middle click or close button click closes the tab
    if (button == 2 || (button == 1 && clickedClose)) {
        if (m_onClose) {
            m_onClose(targetIdx);
        }
        return true;
    }

    // Left click switches to the tab
    if (button == 1 && targetIdx >= 0) {
        if (targetIdx != m_activeIndex && m_onSwitch) {
            m_onSwitch(m_activeIndex, targetIdx);
        }
        return true;
    }

    return false;
}

bool TabStrip::handleScroll(double dx) {
    m_targetScrollOffset = std::clamp(m_targetScrollOffset + dx * 40.0, 0.0, m_maxScrollOffset);
    return true;
}

} // namespace Blueprint::UI
