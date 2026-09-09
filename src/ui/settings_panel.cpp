#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "ui/settings_panel.hpp"
#include "theme/colors.hpp"
#include <SDL2/SDL_keycode.h>
#include <pango/pangocairo.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

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
// Bezier ease for open animation
float easeOut(float t) {
    t = 1.f - t;
    return 1.f - t*t*t;
}
} // namespace

SettingsPanel::SettingsPanel()
    : m_openAnim(260.f, Engine::CubicBezier(0.16f, 1.f, 0.3f, 1.f))
    , m_sectAnim(180.f, Engine::CubicBezier(0.16f, 1.f, 0.3f, 1.f))
{}

void SettingsPanel::toggle() {
    m_wantOpen = !m_wantOpen;
    if (m_wantOpen) m_openAnim.playForward();
    else            m_openAnim.playReverse();
}

void SettingsPanel::setVisible(bool v) {
    if (v == m_wantOpen) return;
    m_wantOpen = v;
    if (v) m_openAnim.playForward();
    else   m_openAnim.playReverse();
}

bool SettingsPanel::isVisible() const {
    return m_openAnim.value() > 0.001f;
}

bool SettingsPanel::wantsRedraw() const {
    return m_openAnim.isRunning() || m_sectAnim.isRunning() ||
           std::abs(m_sidebarCursorY - static_cast<float>(m_section)) > 0.002f ||
           std::abs(m_toggleAnim - (m_settings.anim.enabled ? 1.0f : 0.0f)) > 0.002f;
}

void SettingsPanel::update(float dt) {
    (void)dt;
    // Update open animation speed from settings
    float openDur = 260.f / std::max(0.1f, m_settings.anim.settingsOpen);
    m_openAnim.setDuration(openDur);

    float sectDur = 180.f / std::max(0.1f, m_settings.anim.sectionSwitch);
    m_sectAnim.setDuration(sectDur);

    m_openAnim.update();
    m_sectAnim.update();
    m_openProg  = m_openAnim.value();
    m_sectionProg = m_sectAnim.value();

    // Smooth sidebar cursor interpolation
    float targetY = static_cast<float>(m_section);
    m_sidebarCursorY += (targetY - m_sidebarCursorY) * 0.22f;

    // Smooth toggle animation
    float targetToggle = m_settings.anim.enabled ? 1.0f : 0.0f;
    m_toggleAnim += (targetToggle - m_toggleAnim) * 0.25f;
}

void SettingsPanel::sectionSwitch(int to) {
    if (to == m_section) return;
    m_prevSection = m_section;
    m_sectionDir  = (to > m_section) ? 1.f : -1.f;
    m_section     = to;
    m_sectAnim.setInstant(0.f);
    m_sectAnim.playForward();
}

void SettingsPanel::drawLabel(cairo_t* cr, double x, double y, const char* text,
                               Theme::Color col, float sz, bool bold) {
    PangoLayout* l = pango_cairo_create_layout(cr);
    std::string fdStr = std::string("Inter ") + (bold ? "Bold " : "") + std::to_string(static_cast<int>(sz));
    PangoFontDescription* fd = pango_font_description_from_string(fdStr.c_str());
    pango_layout_set_font_description(l, fd); pango_font_description_free(fd);
    pango_layout_set_text(l, text, -1);
    sc(cr, col);
    cairo_new_path(cr);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, l);
    g_object_unref(l);
}

void SettingsPanel::drawSlider(cairo_t* cr, double x, double y, double w,
                                float value, float minV, float maxV,
                                const char* label, bool hovered, int sliderId) {
    (void)sliderId;
    // Label row
    drawLabel(cr, x, y, label, hovered ? Theme::TEXT_MAIN : Theme::TEXT_MUTED, 10.f);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << value << "x";
    std::string vs = ss.str();
    drawLabel(cr, x + w - 45, y, vs.c_str(), Theme::ACCENT_CALM, 10.f);

    double trackH = 4.0;
    double trackY = y + 22;
    double kR = hovered ? 7.5 : 6.5;

    // Track BG
    rr(cr, x, trackY, w, trackH, trackH / 2.0);
    sc(cr, Theme::BORDER_SOFT);
    cairo_fill(cr);

    // Fill
    float t = (value - minV) / (maxV - minV);
    t = std::clamp(t, 0.0f, 1.0f);
    double fillW = t * w;
    if (fillW > 0.0) {
        rr(cr, x, trackY, fillW, trackH, trackH / 2.0);
        sc(cr, Theme::ACCENT_CALM, 0.90f);
        cairo_fill(cr);
    }

    // Knob with glow
    double kx = x + fillW;
    if (hovered) {
        cairo_new_path(cr);
        cairo_arc(cr, kx, trackY + trackH / 2.0, kR + 3.0, 0, 2 * M_PI);
        sc(cr, Theme::ACCENT_CALM, 0.25f);
        cairo_fill(cr);
    }

    cairo_new_path(cr);
    cairo_arc(cr, kx, trackY + trackH / 2.0, kR, 0, 2 * M_PI);
    sc(cr, Theme::BG_SURFACE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::ACCENT_CALM, hovered ? 1.0f : 0.85f);
    cairo_set_line_width(cr, hovered ? 2.0 : 1.5);
    cairo_stroke(cr);
}

void SettingsPanel::drawToggle(cairo_t* cr, double x, double y, bool /*on*/,
                                const char* label, bool /*hovered*/, int /*toggleId*/) {
    double tw = 40.0, th = 22.0, r = 11.0;

    // Pill background interpolated color
    rr(cr, x, y, tw, th, r);
    float t = std::clamp(m_toggleAnim, 0.0f, 1.0f);
    // Lerp from Theme::BG_ACTIVE (off) to Theme::ACCENT_CALM (on)
    double bgR = Theme::BG_ACTIVE.r + (Theme::ACCENT_CALM.r - Theme::BG_ACTIVE.r) * t;
    double bgG = Theme::BG_ACTIVE.g + (Theme::ACCENT_CALM.g - Theme::BG_ACTIVE.g) * t;
    double bgB = Theme::BG_ACTIVE.b + (Theme::ACCENT_CALM.b - Theme::BG_ACTIVE.b) * t;
    double bgA = Theme::BG_ACTIVE.a + (0.75f - Theme::BG_ACTIVE.a) * t;
    cairo_set_source_rgba(cr, bgR, bgG, bgB, bgA);
    cairo_fill(cr);

    // Thumb smoothly glides
    double thumbMinX = x + r;
    double thumbMaxX = x + tw - r;
    double thumbX = thumbMinX + (thumbMaxX - thumbMinX) * t;

    cairo_new_path(cr);
    cairo_arc(cr, thumbX, y + th / 2.0, r - 3.0, 0, 2 * M_PI);
    double thR = Theme::TEXT_MUTED.r + (1.0 - Theme::TEXT_MUTED.r) * t;
    double thG = Theme::TEXT_MUTED.g + (1.0 - Theme::TEXT_MUTED.g) * t;
    double thB = Theme::TEXT_MUTED.b + (1.0 - Theme::TEXT_MUTED.b) * t;
    cairo_set_source_rgba(cr, thR, thG, thB, 0.95);
    cairo_fill(cr);

    drawLabel(cr, x + tw + 12, y + (th - 13) / 2.0, label, Theme::TEXT_MAIN, 10.f);
}

void SettingsPanel::drawAnimationsSection(cairo_t* cr, double x, double y, double w) {
    double cY = y;
    auto& a = m_settings.anim;

    drawLabel(cr, x, cY, "Animations", Theme::TEXT_MAIN, 13.f, true);
    cY += 26;

    sc(cr, Theme::BORDER_SOFT, 0.3f);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_move_to(cr, x, cY); cairo_line_to(cr, x + w, cY); cairo_stroke(cr);
    cY += 12;

    // Master toggle
    drawToggle(cr, x, cY, a.enabled, "Enable all animations", m_hoveredItem == 200, 200);
    cY += 36;

    if (!a.enabled) {
        drawLabel(cr, x, cY, "Enable animations to configure individual speeds.", Theme::TEXT_DIM, 9.f);
        return;
    }

    // Per-type sliders
    struct { const char* label; float* val; int id; } sliders[] = {
        { "Tab switch slide (x speed)",    &a.tabSlide,      10 },
        { "Tab close animation (x speed)", &a.tabClose,      11 },
        { "Settings panel open (x speed)", &a.settingsOpen,  12 },
        { "Section switch (x speed)",      &a.sectionSwitch, 13 },
    };
    for (auto& s : sliders) {
        bool hov = (m_hoveredItem == s.id);
        drawSlider(cr, x, cY, w, *s.val, 0.25f, 3.0f, s.label, hov, s.id);
        cY += 46;
    }
    drawLabel(cr, x, cY, "1.0x = default speed  |  >1.0x = faster  |  <1.0x = slower",
              Theme::TEXT_DIM, 9.f);
}

void SettingsPanel::drawComingSoon(cairo_t* cr, double x, double y, const char* title) {
    drawLabel(cr, x, y, title, Theme::TEXT_MAIN, 13.f, true);
    drawLabel(cr, x, y + 36, "Coming soon...", Theme::TEXT_DIM, 10.f);
}

void SettingsPanel::drawSidebar(cairo_t* cr, double x, double y, double w, double h) {
    static const char* tabs[] = { "Animations", "Appearance", "Search", "AI Core" };
    int nTabs = 4;
    double tabH = 36.0, gap = 4.0;
    double tabStartY = y + 8.0;

    // Smooth gliding active cursor pill
    double activeY = tabStartY + m_sidebarCursorY * (tabH + gap);
    rr(cr, x + 8.0, activeY, w - 16.0, tabH, 6.0);
    sc(cr, Theme::BG_ACTIVE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::ACCENT_CALM, 0.45f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // Left accent pill on active cursor
    rr(cr, x + 8.0, activeY + 6.0, 3.0, tabH - 12.0, 1.5);
    sc(cr, Theme::ACCENT_CALM);
    cairo_fill(cr);

    // Tab items and labels
    double curY = tabStartY;
    for (int i = 0; i < nTabs; ++i) {
        bool active = (i == m_section);
        bool hov    = (m_hoveredItem == 1000 + i);

        if (!active && hov) {
            rr(cr, x + 8.0, curY, w - 16.0, tabH, 6.0);
            sc(cr, Theme::BG_SUBTLE, 0.9f);
            cairo_fill(cr);
        }

        drawLabel(cr, x + 20.0, curY + (tabH - 13.0) / 2.0, tabs[i],
                  active ? Theme::TEXT_MAIN : (hov ? Theme::TEXT_MAIN : Theme::TEXT_MUTED),
                  10.f, active);

        curY += tabH + gap;
    }
    (void)h;
}

void SettingsPanel::drawContent(cairo_t* cr, double x, double y, double w, double h, int section) {
    // Clipping
    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_clip(cr);

    double cX = x + 16, cY = y + 16, cW = w - 32;

    if (section == 0)      drawAnimationsSection(cr, cX, cY, cW);
    else if (section == 1) drawComingSoon(cr, cX, cY, "Appearance");
    else if (section == 2) drawComingSoon(cr, cX, cY, "Search");
    else if (section == 3) drawComingSoon(cr, cX, cY, "AI Core");

    cairo_restore(cr);
}

void SettingsPanel::drawPanel(cairo_t* cr) {
    // Panel dims
    double pw = m_pw, ph = m_ph, px = m_px, py = m_py;

    // Shadow
    rr(cr, px + 5, py + 5, pw, ph, 12);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
    cairo_fill(cr);

    // Panel BG
    rr(cr, px, py, pw, ph, 12);
    sc(cr, Theme::BG_SURFACE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, 0.6f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // Title bar
    cairo_save(cr);
    rr(cr, px, py, pw, 44, 12);
    cairo_clip(cr);
    rr(cr, px, py, pw, 44, 0);
    sc(cr, Theme::BG_SUBTLE);
    cairo_fill(cr);
    cairo_restore(cr);

    drawLabel(cr, px + 16, py + 15, "Settings", Theme::TEXT_MAIN, 12.f, true);

    // Close × button
    double cX = px + pw - 22, cY = py + 22;
    bool cHov = (m_hoveredItem == 9999);
    if (cHov) {
        cairo_arc(cr, cX, cY, 9, 0, 2*M_PI);
        sc(cr, Theme::BG_ACTIVE); cairo_fill(cr);
        sc(cr, Theme::TEXT_MAIN);
    } else sc(cr, Theme::TEXT_MUTED, 0.6f);
    cairo_set_line_width(cr, 1.6);
    cairo_new_path(cr);
    cairo_move_to(cr, cX-5, cY-5); cairo_line_to(cr, cX+5, cY+5);
    cairo_move_to(cr, cX+5, cY-5); cairo_line_to(cr, cX-5, cY+5);
    cairo_stroke(cr);

    // Divider
    sc(cr, Theme::BORDER_SOFT, 0.35f);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_move_to(cr, px,    py + 44);
    cairo_line_to(cr, px+pw, py + 44);
    cairo_stroke(cr);

    // Body
    double bodyY = py + 44;
    double bodyH = ph - 44;
    double sideW = 130.0;

    drawSidebar(cr, px, bodyY, sideW, bodyH);

    // Vertical divider
    sc(cr, Theme::BORDER_SOFT, 0.3f);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_move_to(cr, px + sideW, bodyY + 10);
    cairo_line_to(cr, px + sideW, bodyY + bodyH - 10);
    cairo_stroke(cr);

    // Animated section content (vertical slide & fade without text layering)
    double contentX = px + sideW + 1;
    double contentW = pw - sideW - 1;

    cairo_save(cr);
    cairo_rectangle(cr, contentX, bodyY, contentW, bodyH);
    cairo_clip(cr);

    float sp = m_sectionProg;
    if (m_sectAnim.isRunning()) {
        constexpr double maxShift = 30.0;
        // Outgoing section moves vertically and fades out
        double outOff = -m_sectionDir * sp * maxShift;
        cairo_push_group(cr);
        drawContent(cr, contentX, bodyY + outOff, contentW, bodyH, m_prevSection);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, std::clamp(1.0f - sp, 0.0f, 1.0f));

        // Incoming section glides in vertically and fades in
        double inOff = m_sectionDir * (1.0f - sp) * maxShift;
        cairo_push_group(cr);
        drawContent(cr, contentX, bodyY + inOff, contentW, bodyH, m_section);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, std::clamp(sp, 0.0f, 1.0f));
    } else {
        drawContent(cr, contentX, bodyY, contentW, bodyH, m_section);
    }
    cairo_restore(cr);

    // Footer
    drawLabel(cr, px + sideW + 14, py + ph - 20,
              "lampa browser v1.0  |  Deep Obsidian",
              Theme::TEXT_DIM, 9.f);
}

void SettingsPanel::draw(cairo_t* cr, double winW, double winH) {
    float p = m_openProg;
    if (p < 0.001f) return;

    // ── Overlay dim ───────────────────────────────────────────────────────────
    cairo_set_source_rgba(cr, 0, 0, 0, 0.5 * p);
    cairo_rectangle(cr, 0, 0, winW, winH);
    cairo_fill(cr);

    // ── Compute panel geometry ────────────────────────────────────────────────
    m_pw = std::min(winW - 80, 700.0);
    m_ph = std::min(winH - 100, 460.0);
    m_px = (winW - m_pw) / 2.0;
    m_py = (winH - m_ph) / 2.0;

    // ── Animate: scale up + fade from center ──────────────────────────────────
    float ease = easeOut(p);
    double scale = 0.88 + 0.12 * ease;
    double alpha = ease;

    cairo_push_group(cr);

    // Transform about panel center
    double pcx = m_px + m_pw / 2.0;
    double pcy = m_py + m_ph / 2.0;
    cairo_translate(cr, pcx, pcy);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -pcx, -pcy);

    drawPanel(cr);

    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, alpha);
}

bool SettingsPanel::isInBounds(double mx, double my) const {
    return isVisible() && mx >= m_px && mx <= m_px + m_pw && my >= m_py && my <= m_py + m_ph;
}

bool SettingsPanel::handleMouseMove(double mx, double my) {
    if (!isVisible()) return false;

    // Continuous slider dragging
    if (m_dragSlider >= 0) {
        float* vals[] = {
            &m_settings.anim.tabSlide,
            &m_settings.anim.tabClose,
            &m_settings.anim.settingsOpen,
            &m_settings.anim.sectionSwitch,
        };
        if (m_dragSlider >= 0 && m_dragSlider < 4 && m_dragSliderW > 0.0) {
            double t = (mx - m_dragSliderX0) / m_dragSliderW;
            t = std::clamp(t, 0.0, 1.0);
            *vals[m_dragSlider] = static_cast<float>(0.25 + t * (3.0 - 0.25));
            return true;
        }
    }

    int old = m_hoveredItem;
    m_hoveredItem = -1;
    if (!isInBounds(mx, my)) return false;

    // Close button
    double cX = m_px + m_pw - 22, cY = m_py + 22;
    if (std::hypot(mx - cX, my - cY) <= 12) { m_hoveredItem = 9999; }

    // Sidebar tabs
    double bodyY = m_py + 44;
    double tabH = 36.0, gap = 4.0;
    double tabStartY = bodyY + 8;
    for (int i = 0; i < 4; ++i) {
        double ty = tabStartY + i * (tabH + gap);
        if (mx >= m_px + 5 && mx <= m_px + 125 && my >= ty && my <= ty + tabH) {
            m_hoveredItem = 1000 + i;
            break;
        }
    }

    // Toggle and slider hitboxes for animations section
    if (m_section == 0) {
        double sideW = 130.0;
        double cX2 = m_px + sideW + 17;
        double cW  = m_pw - sideW - 33;
        double sY  = bodyY + 64;

        // Toggle hitbox
        if (mx >= cX2 - 4 && mx <= cX2 + 220 && my >= sY - 4 && my <= sY + 28) {
            m_hoveredItem = 200;
        }

        // Sliders with generous hitbox
        if (m_settings.anim.enabled) {
            double sliderY = sY + 36;
            for (int si = 0; si < 4; ++si) {
                if (mx >= cX2 - 10 && mx <= cX2 + cW + 10 && my >= sliderY && my <= sliderY + 38) {
                    m_hoveredItem = 10 + si;
                }
                sliderY += 46;
            }
        }
    }

    return old != m_hoveredItem;
}

bool SettingsPanel::handleMouseDown(double mx, double my) {
    if (!isVisible()) return false;

    // Click outside → close
    if (!isInBounds(mx, my)) {
        setVisible(false);
        return true;
    }

    // Close button
    double cX = m_px + m_pw - 22, cY = m_py + 22;
    if (std::hypot(mx - cX, my - cY) <= 12) { setVisible(false); return true; }

    // Sidebar tabs
    double bodyY = m_py + 44;
    double tabH = 36.0, gap = 4.0;
    double tabStartY = bodyY + 8;
    for (int i = 0; i < 4; ++i) {
        double ty = tabStartY + i * (tabH + gap);
        if (mx >= m_px + 5 && mx <= m_px + 125 && my >= ty && my <= ty + tabH) {
            sectionSwitch(i);
            return true;
        }
    }

    // Toggle: animations enabled
    if (m_section == 0) {
        double sideW = 130.0;
        double cX2   = m_px + sideW + 17;
        double sY    = bodyY + 64;

        if (mx >= cX2 - 4 && mx <= cX2 + 220 && my >= sY - 4 && my <= sY + 28) {
            m_settings.anim.enabled = !m_settings.anim.enabled;
            return true;
        }

        // Sliders
        if (m_settings.anim.enabled) {
            double cW  = m_pw - sideW - 33;
            double sliderY = sY + 36;
            float* vals[] = {
                &m_settings.anim.tabSlide,
                &m_settings.anim.tabClose,
                &m_settings.anim.settingsOpen,
                &m_settings.anim.sectionSwitch,
            };
            for (int si = 0; si < 4; ++si) {
                if (mx >= cX2 - 10 && mx <= cX2 + cW + 10 && my >= sliderY && my <= sliderY + 38) {
                    double t = (mx - cX2) / cW;
                    t = std::clamp(t, 0.0, 1.0);
                    *vals[si] = static_cast<float>(0.25 + t * (3.0 - 0.25));
                    m_dragSlider   = si;
                    m_dragSliderX0 = cX2;
                    m_dragSliderW  = cW;
                    return true;
                }
                sliderY += 46;
            }
        }
    }

    return true; // absorb clicks when visible
}

bool SettingsPanel::handleMouseUp(double /*mx*/, double /*my*/) {
    if (m_dragSlider >= 0) {
        m_dragSlider = -1;
        return true;
    }
    return false;
}

bool SettingsPanel::handleKeyPress(uint32_t sym, const char*) {
    if (!isVisible()) return false;
    if (sym == SDLK_ESCAPE) { setVisible(false); return true; }
    return true;
}

} // namespace Blueprint::UI
