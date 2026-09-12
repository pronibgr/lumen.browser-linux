#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "ui/topbar.hpp"
#include "theme/colors.hpp"
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
    r = std::min(r, std::min(w, h) / 2.0);
    cairo_new_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -M_PI/2, 0);
    cairo_arc(cr, x+w-r, y+h-r, r,  0,      M_PI/2);
    cairo_arc(cr, x+r,   y+h-r, r,  M_PI/2, M_PI);
    cairo_arc(cr, x+r,   y+r,   r,  M_PI,   3*M_PI/2);
    cairo_close_path(cr);
}

PangoLayout* makeLayout(cairo_t* cr, const char* font, int wrapW = -1) {
    PangoLayout* l = pango_cairo_create_layout(cr);
    PangoFontDescription* fd = pango_font_description_from_string(font);
    pango_layout_set_font_description(l, fd);
    pango_font_description_free(fd);
    if (wrapW >= 0) {
        pango_layout_set_width(l, wrapW * PANGO_SCALE);
        pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR);
    }
    return l;
}

void showText(cairo_t* cr, PangoLayout* l, const char* text, double x, double y,
              const Theme::Color& c, float a = 1.f) {
    pango_layout_set_text(l, text, -1);
    sc(cr, c, a);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, l);
}

int textH(PangoLayout* l) { int w, h; pango_layout_get_pixel_size(l, &w, &h); (void)w; return h; }
int textW(PangoLayout* l) { int w, h; pango_layout_get_pixel_size(l, &w, &h); (void)h; return w; }

std::string formatBytes(uint64_t bytes) {
    if (bytes == 0) return "0 KB";
    if (bytes < 1024) return "< 1 KB";
    if (bytes < 1024 * 1024) {
        return std::to_string(bytes / 1024) + " KB";
    }
    double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << mb << " MB";
    return oss.str();
}

} // namespace

CompactTopbar::CompactTopbar() {
    Theme::ThemeManager::instance().init();
}

void CompactTopbar::setTabs(const std::vector<std::shared_ptr<Engine::WebTab>>& tabs, int activeIndex) {
    m_tabStrip.setTabs(tabs, activeIndex);
}

void CompactTopbar::setNavState(bool canBack, bool canFwd, bool canReload) {
    m_canGoBack = canBack;
    m_canGoForward = canFwd;
    m_canReload = canReload;
}

void CompactTopbar::setActiveUrl(const std::string& url) {
    m_currentUrl = url;
    m_omnibox.setText(url);
}

void CompactTopbar::setLoadProgress(float p) {
    m_neonProgress.setProgress(p);
}

void CompactTopbar::setTlsInfo(const Engine::TlsCertificateInfo& info) {
    m_tlsInfo = info;
}

void CompactTopbar::setSiteData(uint64_t bytes, bool canClear) {
    m_siteDataBytes = bytes;
    m_canClearData  = canClear;
}

void CompactTopbar::update(float dt) {
    m_neonProgress.update(dt);
    m_tabStrip.update(dt);
    m_settings.update(dt);

    // smooth lerp for cert banner alpha
    float certTarget = m_certBannerOpen ? 1.0f : 0.0f;
    float certSpeed = 1.0f - std::exp(-24.0f * dt);
    m_certBannerAlpha += (certTarget - m_certBannerAlpha) * certSpeed;

    // wait ~2s before popping cert explanation so it doesnt annoy user
    if (m_certBannerOpen && m_hoveredCertTitle) {
        m_certTitleHoverTimer += dt;
    } else {
        m_certTitleHoverTimer = 0.0;
    }

    // smooth lerp for clear data modal alpha
    float modalTarget = m_clearModalOpen ? 1.0f : 0.0f;
    float modalSpeed = 1.0f - std::exp(-22.0f * dt);
    m_clearModalAlpha += (modalTarget - m_clearModalAlpha) * modalSpeed;

    // hover timer for clear data info tip
    if (m_clearModalOpen && m_hoveredClearInfo) {
        m_clearInfoHoverTimer += dt;
    } else {
        m_clearInfoHoverTimer = 0.0;
    }

    // sync search engine template to omnibox
    m_omnibox.setSearchTemplate(m_settings.settings().getActiveSearchTemplate());

    // reload vortex rotation physics: spin when loading or triggered
    float spinMult = m_settings.settings().anim.reloadSpin;
    bool isLoading = (m_neonProgress.getProgress() > 0.001f && m_neonProgress.getProgress() < 0.999f);
    if (isLoading) {
        m_reloadSpinSpeed = std::max(m_reloadSpinSpeed, 7.0f * spinMult);
    }
    if (m_reloadSpinSpeed > 0.01f) {
        m_reloadSpinAngle += m_reloadSpinSpeed * dt;
        if (!isLoading) {
            // decelerate smoothly when loading completes
            float decel = 1.0f - std::exp(-5.5f * dt);
            m_reloadSpinSpeed += (0.0f - m_reloadSpinSpeed) * decel;
            if (m_reloadSpinSpeed < 0.05f) {
                m_reloadSpinSpeed = 0.0f;
            }
        }
        if (m_reloadSpinAngle > 2.0f * float(M_PI)) {
            m_reloadSpinAngle = std::fmod(m_reloadSpinAngle, 2.0f * float(M_PI));
        }
    }
}

bool CompactTopbar::isAnyOverlayActive() const {
    return m_settings.isVisible() || m_omnibox.isPopupOpen() ||
           m_certBannerOpen || (m_certBannerAlpha > 0.01f) ||
           m_clearModalOpen || (m_clearModalAlpha > 0.01f);
}

bool CompactTopbar::wantsRedraw() const {
    float certTarget = m_certBannerOpen ? 1.0f : 0.0f;
    float modalTarget = m_clearModalOpen ? 1.0f : 0.0f;
    return m_settings.wantsRedraw() || m_tabStrip.wantsRedraw() || m_neonProgress.wantsRedraw() ||
           (m_reloadSpinSpeed > 0.05f) ||
           (std::abs(m_certBannerAlpha - certTarget) > 0.002f) ||
           (std::abs(m_clearModalAlpha - modalTarget) > 0.002f) ||
           (m_certTitleHoverTimer > 0.0 && m_certTitleHoverTimer < 2.2) ||
           (m_clearInfoHoverTimer > 0.0 && m_clearInfoHoverTimer < 1.7);
}

void CompactTopbar::updateLayout(double w) {
    m_cachedW = w;
    // Row 2: Omnibox occupies left with margin, up to right action buttons (w - 86)
    m_omniboxX = 14.0;
    m_omniboxW = std::max(50.0, w - m_omniboxX - 86.0);
    m_omniboxY = Theme::ROW1_HEIGHT + (Theme::ROW2_HEIGHT - Theme::OMNIBOX_HEIGHT) / 2.0;

    m_lockX = m_omniboxX + 4.0;
    m_lockY = m_omniboxY + (Theme::OMNIBOX_HEIGHT - 24.0) / 2.0;
    m_lockW = 24.0;
    m_lockH = 24.0;
}

// nav button icons (arrows, refresh, etc)
void CompactTopbar::drawNavBtn(cairo_t* cr, double cx, double cy, int btnIdx, bool enabled) {
    bool hov = (m_hoveredNav == btnIdx);
    double r = 13.0;

    if (hov && enabled) {
        cairo_arc(cr, cx, cy, r, 0, 2*M_PI);
        sc(cr, Theme::BG_ACTIVE);
        cairo_fill(cr);
    }

    float alpha = enabled ? 1.f : 0.3f;
    sc(cr, hov && enabled ? Theme::TEXT_MAIN : Theme::TEXT_MUTED, alpha);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cr, 1.7);

    if (btnIdx == 0) {
        // back button chevron
        cairo_new_path(cr);
        cairo_move_to(cr, cx + 4.5, cy - 5.5);
        cairo_line_to(cr, cx - 3.5, cy);
        cairo_line_to(cr, cx + 4.5, cy + 5.5);
        cairo_stroke(cr);
    } else if (btnIdx == 1) {
        // forward chevron
        cairo_new_path(cr);
        cairo_move_to(cr, cx - 4.5, cy - 5.5);
        cairo_line_to(cr, cx + 3.5, cy);
        cairo_line_to(cr, cx - 4.5, cy + 5.5);
        cairo_stroke(cr);
    } else if (btnIdx == 2) {
        // twin vortex optical reload icon
        cairo_save(cr);
        cairo_translate(cr, cx, cy);
        cairo_rotate(cr, m_reloadSpinAngle);

        // central optical photon dot (lights up on hover)
        cairo_new_path(cr);
        cairo_arc(cr, 0, 0, 1.35, 0, 2 * M_PI);
        if (hov && enabled) {
            sc(cr, Theme::ACCENT_CALM);
        } else {
            sc(cr, enabled ? Theme::TEXT_MAIN : Theme::TEXT_MUTED, alpha);
        }
        cairo_fill(cr);

        // twin balanced orbital vortex arcs
        sc(cr, (hov && enabled) ? Theme::TEXT_MAIN : Theme::TEXT_MUTED, alpha);
        cairo_set_line_width(cr, 1.45);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

        auto drawVortexArc = [&](double endAngle) {
            // curved arc filament
            cairo_new_path(cr);
            cairo_arc(cr, 0, 0, 5.0, endAngle - 2.15, endAngle);
            cairo_stroke(cr);

            // aerodynamic directional micro-fin at arc tip
            double px = 5.0 * std::cos(endAngle);
            double py = 5.0 * std::sin(endAngle);
            double tx = -std::sin(endAngle);
            double ty =  std::cos(endAngle);
            double nx =  std::cos(endAngle);
            double ny =  std::sin(endAngle);

            cairo_new_path(cr);
            cairo_move_to(cr, px - 2.2 * tx + 1.4 * nx, py - 2.2 * ty + 1.4 * ny);
            cairo_line_to(cr, px, py);
            cairo_line_to(cr, px - 2.2 * tx - 1.4 * nx, py - 2.2 * ty - 1.4 * ny);
            cairo_stroke(cr);
        };

        drawVortexArc(-0.15);
        drawVortexArc(-0.15 + M_PI);

        cairo_restore(cr);
    }
}

void CompactTopbar::drawWindowBtn(cairo_t* cr, double cx, double cy, int btnIdx) {
    bool hov = (m_hoveredWinBtn == btnIdx);
    double pillW = 32.0, pillH = 28.0;
    double pillX = cx - pillW / 2.0, pillY = cy - pillH / 2.0;

    if (hov) {
        rr(cr, pillX, pillY, pillW, pillH, 6.0);
        if (btnIdx == 2) {
            cairo_set_source_rgba(cr, 0.88, 0.22, 0.22, 0.90);
        } else {
            sc(cr, Theme::BG_ACTIVE);
        }
        cairo_fill(cr);
    }

    if (hov && btnIdx == 2) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    } else if (hov) {
        sc(cr, Theme::TEXT_MAIN);
    } else {
        sc(cr, Theme::TEXT_MUTED, 0.85f);
    }

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    if (btnIdx == 0) {
        // Minimize: smooth horizontal bar with rounded caps
        cairo_set_line_width(cr, 1.7);
        cairo_new_path(cr);
        cairo_move_to(cr, cx - 5.5, cy + 2.5);
        cairo_line_to(cr, cx + 5.5, cy + 2.5);
        cairo_stroke(cr);
    } else if (btnIdx == 1) {
        // Maximize / Restore: smooth rounded frame(s)
        if (!m_isMaximized) {
            cairo_set_line_width(cr, 1.5);
            rr(cr, cx - 5.0, cy - 5.0, 10.0, 10.0, 2.0);
            cairo_stroke(cr);
        } else {
            // Overlapping dual rounded frames
            cairo_set_line_width(cr, 1.4);
            // Back frame (upper right)
            rr(cr, cx - 2.5, cy - 6.0, 8.5, 8.5, 1.8);
            cairo_stroke(cr);
            // Front frame (lower left)
            rr(cr, cx - 6.0, cy - 2.5, 8.5, 8.5, 1.8);
            if (hov) sc(cr, Theme::BG_ACTIVE);
            else     sc(cr, Theme::BG_SURFACE);
            cairo_fill_preserve(cr);
            if (hov) sc(cr, Theme::TEXT_MAIN);
            else     sc(cr, Theme::TEXT_MUTED, 0.85f);
            cairo_stroke(cr);
        }
    } else if (btnIdx == 2) {
        // Close: diagonal cross with rounded ends
        cairo_set_line_width(cr, 1.6);
        cairo_new_path(cr);
        cairo_move_to(cr, cx - 4.5, cy - 4.5);
        cairo_line_to(cr, cx + 4.5, cy + 4.5);
        cairo_move_to(cr, cx + 4.5, cy - 4.5);
        cairo_line_to(cr, cx - 4.5, cy + 4.5);
        cairo_stroke(cr);
    }
}

void CompactTopbar::drawRow2Btn(cairo_t* cr, double cx, double cy, int btnIdx) {
    bool hov = (m_hoveredRow2Btn == btnIdx);
    bool active = (btnIdx == 1 && m_settings.isVisible());
    double r = 13.0;

    if (hov || active) {
        cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
        sc(cr, hov ? Theme::BG_ACTIVE : Theme::BG_SUBTLE, active ? 0.6f : 1.0f);
        cairo_fill(cr);
    }

    sc(cr, (active || hov) ? Theme::ACCENT_CALM : Theme::TEXT_MUTED);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    if (btnIdx == 0) {
        // '+' New Tab button
        cairo_set_line_width(cr, 1.6);
        cairo_new_path(cr);
        cairo_move_to(cr, cx, cy - 5.0);
        cairo_line_to(cr, cx, cy + 5.0);
        cairo_move_to(cr, cx - 5.0, cy);
        cairo_line_to(cr, cx + 5.0, cy);
        cairo_stroke(cr);
    } else if (btnIdx == 1) {
        // Settings 3-slider icon
        cairo_set_line_width(cr, 1.6);
        for (int i = 0; i < 3; ++i) {
            double ly = cy - 5 + i * 5;
            cairo_new_path(cr);
            cairo_move_to(cr, cx - 7, ly);
            cairo_line_to(cr, cx + 7, ly);
            cairo_stroke(cr);
            double tx = (i == 0) ? cx - 2 : (i == 1) ? cx + 2 : cx;
            cairo_arc(cr, tx, ly, 2.5, 0, 2 * M_PI);
            sc(cr, (active || hov) ? Theme::ACCENT_CALM : Theme::TEXT_MUTED);
            cairo_fill(cr);
            sc(cr, Theme::BG_SURFACE);
            cairo_arc(cr, tx, ly, 1.2, 0, 2 * M_PI);
            cairo_fill(cr);
            sc(cr, (active || hov) ? Theme::ACCENT_CALM : Theme::TEXT_MUTED);
        }
    }
}

void CompactTopbar::setOnion(bool v) {
    m_isOnion = v;
    m_omnibox.setOnion(v);
}

// padlock status icon (green = tls ok, red = broken, muted = internal, onion = Tor circuit active)
void CompactTopbar::drawLockIcon(cairo_t* cr, double x, double y) {
    double w = m_lockW, h = m_lockH;
    double cx = x + w / 2.0, cy = y + h / 2.0;

    // subtle pill on hover or when banner is active
    if (m_hoveredLock || m_certBannerOpen) {
        rr(cr, x, y, w, h, 6.0);
        sc(cr, Theme::BG_ACTIVE);
        cairo_fill(cr);
    }

    if (m_isOnion) {
        // Onion security badge in #7aa2f7 with subtle glow
        Theme::Color onionCol = Theme::Color{0.48f, 0.64f, 0.97f, 1.0f}; // #7aa2f7
        sc(cr, onionCol);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

        cairo_save(cr);
        cairo_translate(cr, cx, cy);

        // Outer contour of onion bulb
        cairo_set_line_width(cr, 1.2);
        cairo_new_path(cr);
        cairo_move_to(cr, 0.0, -4.5);
        cairo_curve_to(cr, -3.8, -4.5, -6.0, -1.5, -6.0, 2.2);
        cairo_curve_to(cr, -6.0, 5.2, -3.5, 7.0, 0.0, 7.0);
        cairo_curve_to(cr, 3.5, 7.0, 6.0, 5.2, 6.0, 2.2);
        cairo_curve_to(cr, 6.0, -1.5, 3.8, -4.5, 0.0, -4.5);
        cairo_close_path(cr);
        cairo_stroke(cr);

        // Inner dashed ring
        const double dashes[] = { 2.5, 1.0 };
        cairo_set_dash(cr, dashes, 2, 0);
        cairo_set_line_width(cr, 1.0);
        cairo_new_path(cr);
        cairo_move_to(cr, 0.0, -2.0);
        cairo_curve_to(cr, -2.2, -2.0, -3.8, -0.2, -3.8, 2.5);
        cairo_curve_to(cr, -3.8, 4.5, -2.2, 5.6, 0.0, 5.6);
        cairo_curve_to(cr, 2.2, 5.6, 3.8, 4.5, 3.8, 2.5);
        cairo_curve_to(cr, 3.8, -0.2, 2.2, -2.0, 0.0, -2.0);
        cairo_close_path(cr);
        cairo_stroke(cr);
        cairo_set_dash(cr, nullptr, 0, 0);

        // Core dot
        cairo_new_path(cr);
        cairo_arc(cr, 0.0, 2.8, 1.3, 0, 2 * M_PI);
        cairo_fill(cr);

        // Top stem sprout
        cairo_set_line_width(cr, 1.2);
        cairo_new_path(cr);
        cairo_move_to(cr, 0.0, -4.5); cairo_line_to(cr, 0.0, -7.0);
        cairo_move_to(cr, -1.5, -6.2); cairo_line_to(cr, 0.0, -7.0); cairo_line_to(cr, 1.5, -6.2);
        cairo_stroke(cr);

        cairo_restore(cr);
        return;
    }

    bool isInternal = (m_currentUrl.empty() || m_currentUrl == "lumen://newtab" ||
                       m_currentUrl == "lumen://null" || m_currentUrl == "lumen://null-tab" ||
                       m_currentUrl == "lumen://error" ||
                       m_currentUrl == "lampa://newtab" || m_currentUrl == "blueprint://newtab" ||
                       m_currentUrl == "about:blank");

    Theme::Color lockCol;
    if (m_isEphemeral || m_currentUrl == "lumen://null" || m_currentUrl == "lumen://null-tab") {
        lockCol = Theme::Color::fromHex(0x3B5C54); // subdued phosphor cyan
    } else if (isInternal) {
        lockCol = Theme::TEXT_MUTED;
    } else if (m_tlsInfo.isHttps && m_tlsInfo.isValid) {
        lockCol = Theme::Color{0.12f, 0.78f, 0.45f, 1.0f}; // emerald green
    } else {
        lockCol = Theme::Color{0.92f, 0.25f, 0.25f, 1.0f}; // crimson red
    }

    sc(cr, lockCol);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    // padlock upper arch
    cairo_set_line_width(cr, 1.6);
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy - 2.5, 3.6, M_PI, 2 * M_PI);
    cairo_stroke(cr);

    // padlock lower body box
    rr(cr, cx - 5.5, cy - 1.5, 11.0, 8.5, 2.0);
    sc(cr, lockCol);
    cairo_fill(cr);

    // keyhole center dot
    sc(cr, Theme::BG_SURFACE);
    cairo_arc(cr, cx, cy + 2.2, 1.2, 0, 2 * M_PI);
    cairo_fill(cr);
}

// ─────────────────────────── row 1: tabs ──────────────────────────────────
void CompactTopbar::drawRow1(cairo_t* cr, double w) {
    double rowH = Theme::ROW1_HEIGHT;
    double r = m_isMaximized ? 0.0 : 12.0;

    cairo_save(cr);

    // Rounded top corners path
    cairo_new_path(cr);
    if (r > 0.0) {
        cairo_move_to(cr, 0, rowH);
        cairo_line_to(cr, 0, r);
        cairo_arc(cr, r, r, r, M_PI, 3 * M_PI / 2);
        cairo_line_to(cr, w - r, 0);
        cairo_arc(cr, w - r, r, r, -M_PI / 2, 0);
        cairo_line_to(cr, w, rowH);
        cairo_close_path(cr);
    } else {
        cairo_rectangle(cr, 0, 0, w, rowH);
    }

    sc(cr, Theme::BG_SURFACE);
    cairo_fill_preserve(cr);
    cairo_clip(cr);

    // nav buttons: 3x32px (x = 0..96)
    double navCY = rowH / 2.0;
    drawNavBtn(cr, 18,  navCY, 0, m_canGoBack);
    drawNavBtn(cr, 50,  navCY, 1, m_canGoForward);
    drawNavBtn(cr, 82,  navCY, 2, m_canReload);

    // window controls on far right: 3 buttons (Minimize, Maximize/Restore, Close)
    // each 40px wide from w - 120 to w
    double winCY = rowH / 2.0;
    drawWindowBtn(cr, w - 100.0, winCY, 0); // Minimize
    drawWindowBtn(cr, w - 60.0,  winCY, 1); // Maximize/Restore
    drawWindowBtn(cr, w - 20.0,  winCY, 2); // Close

    // tab strip: squished between nav buttons (96px) and window controls (120px)
    double tabsX = 96.0;
    double winW  = 120.0;
    double tabsW = std::max(50.0, w - tabsX - winW);
    m_tabStrip.draw(cr, tabsX, 0, tabsW, rowH);

    // bottom border line
    sc(cr, Theme::BORDER_SOFT, 0.35f);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_move_to(cr, 0, rowH - 0.5);
    cairo_line_to(cr, w, rowH - 0.5);
    cairo_stroke(cr);

    cairo_restore(cr);

    // Subtle top border highlight on rounded corners
    if (r > 0.0) {
        cairo_new_path(cr);
        cairo_move_to(cr, 0, rowH);
        cairo_line_to(cr, 0, r);
        cairo_arc(cr, r, r, r, M_PI, 3 * M_PI / 2);
        cairo_line_to(cr, w - r, 0);
        cairo_arc(cr, w - r, r, r, -M_PI / 2, 0);
        cairo_line_to(cr, w, rowH);
        sc(cr, Theme::BORDER_SOFT, 0.45f);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }
}

// ─────────────────────────── row 2: omnibox & lock ────────────────────────
void CompactTopbar::drawRow2(cairo_t* cr, double w) {
    double rowY = Theme::ROW1_HEIGHT;
    double rowH = Theme::ROW2_HEIGHT;
    double rowCY = rowY + rowH / 2.0;

    // background
    cairo_rectangle(cr, 0, rowY, w, rowH);
    sc(cr, Theme::BG_ABYSS);
    cairo_fill(cr);

    // omnibox with offset for the lock icon
    m_omnibox.draw(cr, m_omniboxX, m_omniboxY, m_omniboxW, Theme::OMNIBOX_HEIGHT);

    // lock icon inside omnibox on the left
    drawLockIcon(cr, m_lockX, m_lockY);

    // Row 2 action buttons on right: New tab (+) and Settings
    drawRow2Btn(cr, w - 58.0, rowCY, 0); // New tab (+)
    drawRow2Btn(cr, w - 24.0, rowCY, 1); // Settings

    // neon progress bar on page load
    m_neonProgress.draw(cr, 0, rowY + rowH - 2.0, w);

    // bottom border
    sc(cr, Theme::BORDER_SOFT, 0.5f);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_move_to(cr, 0, rowY + rowH - 0.5);
    cairo_line_to(cr, w, rowY + rowH - 0.5);
    cairo_stroke(cr);
}

void CompactTopbar::draw(cairo_t* cr, double w, double h) {
    (void)h;
    if (m_cachedW != w) updateLayout(w);
    drawRow1(cr, w);
    drawRow2(cr, w);
}

// ─────────────────────────── security & cert banner ────────────────────────
void CompactTopbar::drawCertBanner(cairo_t* cr, double winW, double winH) {
    (void)winW; (void)winH;
    if (m_certBannerAlpha <= 0.01f) return;

    float alpha = m_certBannerAlpha;
    double banX = m_omniboxX + 4.0;
    double banY = m_omniboxY + Theme::OMNIBOX_HEIGHT + 6.0;
    double banW = 390.0, banH = 175.0;

    cairo_push_group(cr);

    // drop shadow behind popup
    rr(cr, banX + 3, banY + 3, banW, banH, 10.0);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
    cairo_fill(cr);

    // main popup background box
    rr(cr, banX, banY, banW, banH, 10.0);
    sc(cr, Theme::BG_POPUP);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, 0.7f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // 1. security status header row
    double iconCX = banX + 26, iconCY = banY + 24;
    bool isInternal = (m_currentUrl.empty() || m_currentUrl == "lumen://newtab" ||
                       m_currentUrl == "lumen://null" || m_currentUrl == "lumen://null-tab" ||
                       m_currentUrl == "lumen://error" ||
                       m_currentUrl == "lampa://newtab" || m_currentUrl == "blueprint://newtab" ||
                       m_currentUrl == "about:blank");
    bool isSecure = m_tlsInfo.isHttps && m_tlsInfo.isValid && !isInternal;
    bool isNullPage = (m_isEphemeral || m_currentUrl == "lumen://null" || m_currentUrl == "lumen://null-tab");

    Theme::Color statusCol = isNullPage ? Theme::Color::fromHex(0x3B5C54)
                             : (isInternal ? Theme::TEXT_MUTED
                             : (isSecure ? Theme::Color{0.12f, 0.78f, 0.45f, 1.0f}
                             : Theme::Color{0.92f, 0.25f, 0.25f, 1.0f}));

    // lock icon badge
    cairo_arc(cr, iconCX, iconCY - 3, 4.0, M_PI, 2 * M_PI);
    sc(cr, statusCol); cairo_set_line_width(cr, 1.7); cairo_stroke(cr);
    rr(cr, iconCX - 6, iconCY - 2, 12, 9, 2);
    sc(cr, statusCol); cairo_fill(cr);

    // header title
    const char* titleText = isNullPage ? "[∅] L.NULL // RAM ONLY"
                          : (isInternal ? "Internal Page"
                          : (isSecure ? "Connection is secure"
                          : "Connection not secure"));
    PangoLayout* lTitle = makeLayout(cr, "Inter Bold 11");
    showText(cr, lTitle, titleText, banX + 46, banY + 16, Theme::TEXT_MAIN);

    // 2. certificate status text and issuer pill
    PangoLayout* lSub = makeLayout(cr, "Inter 9");
    const char* subText = isNullPage ? "Volatile Ephemeral Session:"
                        : (isInternal ? "Browser internal resource:"
                        : (isSecure ? "Certificate verified:"
                        : "Security status:"));
    showText(cr, lSub, subText, banX + 46, banY + 44, Theme::TEXT_MUTED);

    // badge pill
    std::string pillLabel;
    Theme::Color pillTextColor;
    if (isNullPage) {
        pillLabel = "CACHE: PURGED · COOKIES: ISOLATED";
        pillTextColor = Theme::Color::fromHex(0x3B5C54);
    } else if (isInternal) {
        pillLabel = "Local page";
        pillTextColor = Theme::TEXT_MUTED;
    } else if (isSecure) {
        pillLabel = m_tlsInfo.issuer.empty() ? "Certificate Authority" : m_tlsInfo.issuer;
        pillTextColor = Theme::Color{0.38f, 0.65f, 0.98f, 1.0f};
    } else {
        pillLabel = "Certificate invalid or missing";
        pillTextColor = Theme::Color{0.95f, 0.45f, 0.45f, 1.0f};
    }

    PangoLayout* lCert = makeLayout(cr, "Inter Bold 9");
    pango_layout_set_text(lCert, pillLabel.c_str(), -1);
    int cw = textW(lCert);
    double pillW = cw + 18.0, pillH = 22.0;
    double pillX = banX + 46, pillY = banY + 62.0;

    rr(cr, pillX, pillY, pillW, pillH, 5.0);
    if (isInternal) {
        cairo_set_source_rgba(cr, 0.16, 0.20, 0.28, 0.7);
        cairo_fill_preserve(cr);
        sc(cr, Theme::BORDER_SOFT, 0.6f);
    } else if (isSecure) {
        cairo_set_source_rgba(cr, 0.10, 0.22, 0.46, 0.85); // calm navy tint
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0.25, 0.48, 0.92, 0.65);
    } else {
        cairo_set_source_rgba(cr, 0.45, 0.12, 0.12, 0.75); // warning red tint
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0.85, 0.25, 0.25, 0.65);
    }
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    showText(cr, lCert, pillLabel.c_str(), pillX + 9, pillY + 4.5, pillTextColor);

    // 3. divider line
    double sepY = banY + 104.0;
    cairo_set_line_width(cr, 1.0);
    sc(cr, Theme::BORDER_SOFT, 0.45f);
    cairo_new_path(cr);
    cairo_move_to(cr, banX + 16, sepY);
    cairo_line_to(cr, banX + banW - 16, sepY);
    cairo_stroke(cr);

    // 4. clear site data action button
    double btnX = banX + 16, btnY = banY + 120.0;
    double btnW = banW - 32, btnH = 34.0;
    rr(cr, btnX, btnY, btnW, btnH, 6.0);

    if (!m_canClearData) {
        // disabled state when nothing to clear or on internal pages
        sc(cr, Theme::BG_SUBTLE, 0.35f);
        cairo_fill_preserve(cr);
        sc(cr, Theme::BORDER_SOFT, 0.3f);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        // subtle dimmed trash icon
        double trX = btnX + 16, trY = btnY + btnH / 2.0;
        sc(cr, Theme::TEXT_MUTED, 0.4f);
        cairo_set_line_width(cr, 1.4);
        cairo_rectangle(cr, trX - 4, trY - 3, 8, 8);
        cairo_stroke(cr);
        cairo_move_to(cr, trX - 6, trY - 4); cairo_line_to(cr, trX + 6, trY - 4);
        cairo_stroke(cr);

        PangoLayout* lBtn = makeLayout(cr, "Inter SemiBold 10");
        showText(cr, lBtn, "Clear site data (unavailable for this page)", btnX + 32, btnY + 9, Theme::TEXT_MUTED, 0.45f);
        g_object_unref(lBtn);
    } else {
        if (m_hoveredClearSiteBtn) {
            sc(cr, Theme::BG_ACTIVE);
            cairo_fill_preserve(cr);
            sc(cr, Theme::BORDER_SOFT, 0.7f);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        } else {
            sc(cr, Theme::BG_SUBTLE, 0.6f);
            cairo_fill(cr);
        }

        // trashcan icon
        double trX = btnX + 16, trY = btnY + btnH / 2.0;
        sc(cr, Theme::ACCENT_CALM);
        cairo_set_line_width(cr, 1.4);
        cairo_rectangle(cr, trX - 4, trY - 3, 8, 8);
        cairo_stroke(cr);
        cairo_move_to(cr, trX - 6, trY - 4); cairo_line_to(cr, trX + 6, trY - 4);
        cairo_stroke(cr);

        PangoLayout* lBtn = makeLayout(cr, "Inter SemiBold 10");
        showText(cr, lBtn, "Clear data stored by this site", btnX + 32, btnY + 9, Theme::TEXT_MAIN);
        g_object_unref(lBtn);
    }

    // 5. connection tooltip when hovering title for a moment
    if (m_certTitleHoverTimer >= 1.5) {
        const char* tipText = isSecure
            ? "Your data (passwords, payment cards, messages) is encrypted with TLS and private in transit."
            : "Data sent to this site is unencrypted and could potentially be intercepted by network sniffers.";
        PangoLayout* lTip = makeLayout(cr, "Inter 9", 280);
        pango_layout_set_text(lTip, tipText, -1);
        int tipH = textH(lTip);
        double tipX = banX + 46, tipY = banY + 38.0;

        rr(cr, tipX, tipY, 296, tipH + 14, 6.0);
        sc(cr, Theme::BG_SURFACE); cairo_fill_preserve(cr);
        sc(cr, Theme::BORDER_SOFT, 0.9f); cairo_set_line_width(cr, 1.0); cairo_stroke(cr);
        showText(cr, lTip, tipText, tipX + 8, tipY + 7, Theme::TEXT_MAIN);
        g_object_unref(lTip);
    }

    g_object_unref(lTitle);
    g_object_unref(lSub);
    g_object_unref(lCert);

    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, alpha);
}

// ─────────────────────────── clear data modal dialog ───────────────────────
void CompactTopbar::drawClearDataModal(cairo_t* cr, double winW, double winH) {
    if (m_clearModalAlpha <= 0.01f) return;

    float alpha = m_clearModalAlpha;

    // full screen backdrop dim
    cairo_set_source_rgba(cr, 0, 0, 0, 0.65 * alpha);
    double r = m_isMaximized ? 0.0 : 12.0;
    if (r > 0.0) {
        rr(cr, 0, 0, winW, winH, r);
    } else {
        cairo_rectangle(cr, 0, 0, winW, winH);
    }
    cairo_fill(cr);

    // center popup box
    double mw = 480.0, mh = 310.0;
    double mx = (winW - mw) / 2.0, my = (winH - mh) / 2.0;

    cairo_push_group(cr);

    // soft shadow behind card
    rr(cr, mx + 6, my + 6, mw, mh, 14.0);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    cairo_fill(cr);

    // modal body card
    rr(cr, mx, my, mw, mh, 14.0);
    sc(cr, Theme::BG_SURFACE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, 0.7f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // warning badge header
    rr(cr, mx + 20, my + 18, 32, 32, 8.0);
    sc(cr, Theme::Color{0.95f, 0.25f, 0.25f, 0.15f});
    cairo_fill(cr);

    PangoLayout* lWarn = makeLayout(cr, "Inter Bold 14");
    showText(cr, lWarn, "!", mx + 32, my + 22, Theme::Color{0.95f, 0.35f, 0.35f, 1.0f});

    PangoLayout* lTitle = makeLayout(cr, "Inter Bold 13");
    showText(cr, lTitle, "Clear Website Data", mx + 62, my + 18, Theme::TEXT_MAIN);

    std::string siteHost = m_tlsInfo.subject.empty() ? "current site" : m_tlsInfo.subject;
    PangoLayout* lHost = makeLayout(cr, "Inter 10");
    std::string hostStr = "Host: " + siteHost;
    showText(cr, lHost, hostStr.c_str(), mx + 62, my + 36, Theme::TEXT_MUTED);

    // inner details card
    double cardX = mx + 20, cardY = my + 64;
    double cardW = mw - 40, cardH = 112.0;
    rr(cr, cardX, cardY, cardW, cardH, 8.0);
    sc(cr, Theme::BG_SUBTLE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, 0.4f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // disk consumption report
    std::string sizeStr = m_canClearData ? formatBytes(m_siteDataBytes) : "0 KB";
    PangoLayout* lSize = makeLayout(cr, "Inter Bold 12");
    showText(cr, lSize, ("Total storage: " + sizeStr).c_str(), cardX + 16, cardY + 12, Theme::ACCENT_CALM);

    PangoLayout* lItem = makeLayout(cr, "Inter 10");
    if (m_canClearData) {
        showText(cr, lItem, "• Cookies: active login & tracking tokens", cardX + 16, cardY + 36, Theme::TEXT_MAIN);
        showText(cr, lItem, "• Web storage: LocalStorage, IndexedDB data", cardX + 16, cardY + 56, Theme::TEXT_MUTED);
        showText(cr, lItem, "• Cached assets: downloaded scripts, fonts & images", cardX + 16, cardY + 76, Theme::TEXT_MUTED);
    } else {
        showText(cr, lItem, "• No local data present", cardX + 16, cardY + 36, Theme::TEXT_MUTED);
        showText(cr, lItem, "• No cookies recorded", cardX + 16, cardY + 56, Theme::TEXT_MUTED);
        showText(cr, lItem, "• No cached resources found", cardX + 16, cardY + 76, Theme::TEXT_MUTED);
    }

    // disclaimer warning
    PangoLayout* lWarnText = makeLayout(cr, "Inter SemiBold 9.5", static_cast<int>(cardW));
    const char* warnStr = m_canClearData
        ? "Are you sure you want to clear stored data for this website? NOTE: you will be signed out."
        : "Cannot clear data: this page is an internal URL or failed to load.";
    pango_layout_set_text(lWarnText, warnStr, -1);
    showText(cr, lWarnText, warnStr, mx + 20, my + 192,
             m_canClearData ? Theme::Color{0.95f, 0.35f, 0.35f, 1.0f} : Theme::TEXT_MUTED);

    // subtle line divider
    sc(cr, Theme::BORDER_SOFT, 0.4f);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, mx + 20, my + 242);
    cairo_line_to(cr, mx + mw - 20, my + 242);
    cairo_stroke(cr);

    // info icon hover circle
    double infoX = mx + 22, infoY = my + mh - 44;
    cairo_arc(cr, infoX + 10, infoY + 14, 10.0, 0, 2 * M_PI);
    if (m_hoveredClearInfo) {
        sc(cr, Theme::ACCENT_CALM, 0.25f); cairo_fill_preserve(cr);
        sc(cr, Theme::ACCENT_CALM); cairo_set_line_width(cr, 1.4); cairo_stroke(cr);
    } else {
        sc(cr, Theme::BG_SUBTLE); cairo_fill_preserve(cr);
        sc(cr, Theme::TEXT_MUTED, 0.5f); cairo_set_line_width(cr, 1.0); cairo_stroke(cr);
    }
    PangoLayout* lI = makeLayout(cr, "Inter Bold 10");
    showText(cr, lI, "i", infoX + 7.5, infoY + 6.5, m_hoveredClearInfo ? Theme::ACCENT_CALM : Theme::TEXT_MUTED);

    // action buttons: delete (red) and cancel (subtle)
    double btnDelX = mx + mw - 195, btnDelY = my + mh - 46;
    double btnDelW = 85.0, btnDelH = 30.0;
    rr(cr, btnDelX, btnDelY, btnDelW, btnDelH, 6.0);
    if (!m_canClearData) {
        cairo_set_source_rgba(cr, 0.35, 0.15, 0.15, 0.45); // disabled look
    } else if (m_hoveredClearConfirm) {
        cairo_set_source_rgba(cr, 0.75, 0.12, 0.12, 1.0); // darker on hover
    } else {
        cairo_set_source_rgba(cr, 0.88, 0.16, 0.16, 1.0);
    }
    cairo_fill(cr);

    // center text for delete button
    PangoLayout* lDel = makeLayout(cr, "Inter Bold 10");
    pango_layout_set_text(lDel, "Delete", -1);
    int dw = 0, dh = 0;
    pango_layout_get_pixel_size(lDel, &dw, &dh);
    double txDel = btnDelX + (btnDelW - dw) / 2.0;
    double tyDel = btnDelY + (btnDelH - dh) / 2.0;
    showText(cr, lDel, "Delete", txDel, tyDel, Theme::Color{1, 1, 1, m_canClearData ? 1.0f : 0.4f});

    // center text for cancel button
    double btnCanX = mx + mw - 95, btnCanY = my + mh - 46;
    double btnCanW = 75.0, btnCanH = 30.0;
    rr(cr, btnCanX, btnCanY, btnCanW, btnCanH, 6.0);
    if (m_hoveredClearCancel) {
        cairo_set_source_rgba(cr, 0.12, 0.35, 0.85, 1.0); // blue hover
    } else {
        cairo_set_source_rgba(cr, 0.15, 0.42, 0.95, 1.0); // blue
    }
    cairo_fill(cr);

    PangoLayout* lCan = makeLayout(cr, "Inter Bold 10");
    pango_layout_set_text(lCan, "Cancel", -1);
    int cw_can = 0, ch_can = 0;
    pango_layout_get_pixel_size(lCan, &cw_can, &ch_can);
    double txCan = btnCanX + (btnCanW - cw_can) / 2.0;
    double tyCan = btnCanY + (btnCanH - ch_can) / 2.0;
    showText(cr, lCan, "Cancel", txCan, tyCan, Theme::Color{1, 1, 1, 1});

    // tooltip on little info icon hover
    if (m_clearInfoHoverTimer >= 1.5) {
        const char* tipInfo = "Cleared data may include saved login sessions, cached preferences, local database files and temporary cookies.";
        PangoLayout* lTip = makeLayout(cr, "Inter 9", 260);
        pango_layout_set_text(lTip, tipInfo, -1);
        int th = textH(lTip);
        double tipX = infoX + 26, tipY = infoY - th - 8;

        rr(cr, tipX, tipY, 276, th + 14, 6.0);
        sc(cr, Theme::BG_SURFACE); cairo_fill_preserve(cr);
        sc(cr, Theme::BORDER_SOFT, 0.9f); cairo_set_line_width(cr, 1.0); cairo_stroke(cr);
        showText(cr, lTip, tipInfo, tipX + 8, tipY + 7, Theme::TEXT_MAIN);
        g_object_unref(lTip);
    }

    g_object_unref(lWarn);
    g_object_unref(lTitle);
    g_object_unref(lHost);
    g_object_unref(lSize);
    g_object_unref(lItem);
    g_object_unref(lWarnText);
    g_object_unref(lI);
    g_object_unref(lDel);
    g_object_unref(lCan);

    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, alpha);
}

void CompactTopbar::drawOverlays(cairo_t* cr, double winW, double winH) {
    cairo_save(cr);
    double r = m_isMaximized ? 0.0 : 12.0;
    if (r > 0.0) {
        cairo_new_path(cr);
        cairo_arc(cr, winW - r, r, r, -M_PI / 2, 0);
        cairo_arc(cr, winW - r, winH - r, r, 0, M_PI / 2);
        cairo_arc(cr, r, winH - r, r, M_PI / 2, M_PI);
        cairo_arc(cr, r, r, r, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr);
        cairo_clip(cr);
    }

    // 1. Omnibox dropdown (below row 2)
    m_omnibox.drawPopup(cr, m_omniboxX, m_omniboxY + Theme::OMNIBOX_HEIGHT + 4, m_omniboxW);

    // 2. Security & Certificate Banner
    drawCertBanner(cr, winW, winH);

    // 3. Clear Data Modal Dialog
    drawClearDataModal(cr, winW, winH);

    // 4. Settings Panel
    m_settings.draw(cr, winW, winH);

    cairo_restore(cr);
}

bool CompactTopbar::handleMouseMove(double mx, double my) {
    if (m_settings.isVisible()) {
        return m_settings.handleMouseMove(mx, my);
    }

    // modal dialog hover check
    if (m_clearModalOpen) {
        double mw = 480.0, mh = 310.0;
        double winW = (m_cachedW > 0) ? m_cachedW : 1280.0;
        double winH = 800.0;
        double bx = (winW - mw) / 2.0, by = (winH - mh) / 2.0;

        double btnDelX = bx + mw - 195, btnDelY = by + mh - 46;
        bool hovDel = m_canClearData && (mx >= btnDelX && mx <= btnDelX + 85 && my >= btnDelY && my <= btnDelY + 30);

        double btnCanX = bx + mw - 95, btnCanY = by + mh - 46;
        bool hovCan = (mx >= btnCanX && mx <= btnCanX + 75 && my >= btnCanY && my <= btnCanY + 30);

        double infoX = bx + 22, infoY = by + mh - 44;
        bool hovInfo = (std::hypot(mx - (infoX + 10), my - (infoY + 14)) <= 14);

        bool changed = (hovDel != m_hoveredClearConfirm || hovCan != m_hoveredClearCancel || hovInfo != m_hoveredClearInfo);
        m_hoveredClearConfirm = hovDel;
        m_hoveredClearCancel  = hovCan;
        m_hoveredClearInfo    = hovInfo;
        return changed;
    }

    // cert banner hover check
    if (m_certBannerOpen) {
        double banX = m_omniboxX + 4.0;
        double banY = m_omniboxY + Theme::OMNIBOX_HEIGHT + 6.0;
        double banW = 390.0, banH = 175.0;
        (void)banH;

        bool hovTitle = (mx >= banX + 46 && mx <= banX + 280 && my >= banY + 12 && my <= banY + 36);
        double btnX = banX + 16, btnY = banY + 120.0;
        bool hovBtn = m_canClearData && (mx >= btnX && mx <= btnX + (banW - 32) && my >= btnY && my <= btnY + 34);

        bool changed = (hovTitle != m_hoveredCertTitle || hovBtn != m_hoveredClearSiteBtn);
        m_hoveredCertTitle = hovTitle;
        m_hoveredClearSiteBtn = hovBtn;
        return changed;
    }

    int oldNav  = m_hoveredNav;
    int oldWin  = m_hoveredWinBtn;
    int oldRow2 = m_hoveredRow2Btn;
    m_hoveredNav     = -1;
    m_hoveredWinBtn  = -1;
    m_hoveredRow2Btn = -1;

    double r = 14.0;
    if (my <= Theme::ROW1_HEIGHT) {
        // Nav buttons (0..96)
        struct { double cx; double cy; int idx; } navPts[] = {
            {18,  double(Theme::ROW1_HEIGHT)/2.0, 0},
            {50,  double(Theme::ROW1_HEIGHT)/2.0, 1},
            {82,  double(Theme::ROW1_HEIGHT)/2.0, 2},
        };
        for (auto& p : navPts) {
            if (std::hypot(mx - p.cx, my - p.cy) <= r) {
                m_hoveredNav = p.idx; break;
            }
        }

        // Window buttons (w - 120..w)
        if (m_cachedW > 0 && mx >= m_cachedW - 120.0) {
            if (mx < m_cachedW - 80.0) {
                m_hoveredWinBtn = 0; // Minimize
            } else if (mx < m_cachedW - 40.0) {
                m_hoveredWinBtn = 1; // Maximize
            } else {
                m_hoveredWinBtn = 2; // Close
            }
        }
    } else if (my <= Theme::ROW1_HEIGHT + Theme::ROW2_HEIGHT) {
        // Row 2 action buttons on right:
        double row2CY = double(Theme::ROW1_HEIGHT) + double(Theme::ROW2_HEIGHT) / 2.0;
        if (m_cachedW > 0) {
            if (std::hypot(mx - (m_cachedW - 58.0), my - row2CY) <= r) {
                m_hoveredRow2Btn = 0; // New Tab (+)
            } else if (std::hypot(mx - (m_cachedW - 24.0), my - row2CY) <= r) {
                m_hoveredRow2Btn = 1; // Settings
            }
        }
    }

    // lock icon hover check
    bool oldLock = m_hoveredLock;
    m_hoveredLock = (mx >= m_lockX && mx <= m_lockX + m_lockW &&
                     my >= m_lockY && my <= m_lockY + m_lockH);

    bool ch = (oldNav != m_hoveredNav || oldWin != m_hoveredWinBtn ||
               oldRow2 != m_hoveredRow2Btn || oldLock != m_hoveredLock);
    if (m_tabStrip.handleMouseMove(mx, my)) ch = true;
    if (m_omnibox.handleMouseMove(mx, my))  ch = true;
    return ch;
}

bool CompactTopbar::handleMouseDown(double mx, double my) {
    if (m_settings.isVisible()) {
        return m_settings.handleMouseDown(mx, my);
    }

    // modal dialog click handling
    if (m_clearModalOpen) {
        if (m_hoveredClearConfirm && m_canClearData) {
            m_clearModalOpen = false;
            if (m_onClearData) m_onClearData();
            return true;
        }
        if (m_hoveredClearCancel) {
            m_clearModalOpen = false;
            return true;
        }
        // click outside dialog box closes modal
        double mw = 480.0, mh = 310.0;
        double winW = (m_cachedW > 0) ? m_cachedW : 1280.0;
        double winH = 800.0;
        double bx = (winW - mw) / 2.0, by = (winH - mh) / 2.0;
        if (mx < bx || mx > bx + mw || my < by || my > by + mh) {
            m_clearModalOpen = false;
            return true;
        }
        return true; // absorb click inside modal so it doesn't bleed to the webview
    }

    // cert banner click handling
    if (m_certBannerOpen) {
        if (m_hoveredClearSiteBtn && m_canClearData) {
            m_certBannerOpen = false;
            m_clearModalOpen = true;
            return true;
        }
        double banX = m_omniboxX + 4.0;
        double banY = m_omniboxY + Theme::OMNIBOX_HEIGHT + 6.0;
        double banW = 390.0, banH = 175.0;
        if (mx < banX || mx > banX + banW || my < banY || my > banY + banH) {
            // clicked outside banner
            m_certBannerOpen = false;
            // if clicked directly on lock icon, toggle it
            if (mx >= m_lockX && mx <= m_lockX + m_lockW && my >= m_lockY && my <= m_lockY + m_lockH) {
                return true;
            }
        } else {
            return true; // absorb click inside banner
        }
    }

    // lock button click
    if (mx >= m_lockX && mx <= m_lockX + m_lockW && my >= m_lockY && my <= m_lockY + m_lockH) {
        m_certBannerOpen = !m_certBannerOpen;
        if (m_certBannerOpen && m_onRefreshSiteData) {
            m_onRefreshSiteData();
        }
        return true;
    }

    // 1. window buttons in Row 1 (Minimize, Maximize/Restore, Close)
    if (my <= Theme::ROW1_HEIGHT && m_cachedW > 0 && mx >= m_cachedW - 120.0) {
        if (mx < m_cachedW - 80.0) {
            if (m_onMinimize) m_onMinimize();
            return true;
        } else if (mx < m_cachedW - 40.0) {
            if (m_onMaximizeToggle) m_onMaximizeToggle();
            return true;
        } else {
            if (m_onCloseWindow) m_onCloseWindow();
            return true;
        }
    }

    // 2. Row 1 navigation buttons
    if (m_hoveredNav == 0 && m_canGoBack    && m_onBack)    { m_onBack();    return true; }
    if (m_hoveredNav == 1 && m_canGoForward && m_onForward) { m_onForward(); return true; }
    if (m_hoveredNav == 2 && m_canReload    && m_onReload)  {
        float spinMult = m_settings.settings().anim.reloadSpin;
        m_reloadSpinSpeed = 16.0f * spinMult; // spin impulse on click
        m_onReload();
        return true;
    }

    // 3. Row 1 tab strip (nav and window buttons already handled above)
    if (my <= Theme::ROW1_HEIGHT) {
        if (mx >= 96.0 && mx < m_cachedW - 120.0) {
            if (m_tabStrip.handleMouseDown(mx, my, 1)) {
                m_omnibox.setFocused(false);
                return true;
            }
        }
        return false;
    }

    // 4. Row 2 action buttons (+ and Settings)
    if (my > Theme::ROW1_HEIGHT && my <= Theme::ROW1_HEIGHT + Theme::ROW2_HEIGHT) {
        double r = 14.0;
        double row2CY = double(Theme::ROW1_HEIGHT) + double(Theme::ROW2_HEIGHT) / 2.0;
        if (m_cachedW > 0) {
            if (std::hypot(mx - (m_cachedW - 58.0), my - row2CY) <= r || m_hoveredRow2Btn == 0) {
                if (m_onNewTab) m_onNewTab();
                return true;
            }
            if (std::hypot(mx - (m_cachedW - 24.0), my - row2CY) <= r || m_hoveredRow2Btn == 1) {
                m_settings.toggle();
                return true;
            }
        }
    }

    // 5. Row 2 omnibox (single click focus and text editing)
    if (m_omnibox.handleMouseDown(mx, my)) {
        return true;
    }
    return false;
}

bool CompactTopbar::handleMouseUp(double mx, double my) {
    bool handled = false;
    if (m_settings.isVisible()) {
        handled = m_settings.handleMouseUp(mx, my) || handled;
    }
    handled = m_omnibox.handleMouseUp(mx, my) || handled;
    return handled;
}

bool CompactTopbar::handleMouseWheel(double dx) {
    return m_tabStrip.handleScroll(dx);
}

bool CompactTopbar::handleKeyPress(uint32_t sym, uint32_t mod, const char* text) {
    if (m_clearModalOpen) {
        m_clearModalOpen = false;
        return true;
    }
    if (m_certBannerOpen) {
        m_certBannerOpen = false;
        return true;
    }
    if (m_settings.isVisible()) return m_settings.handleKeyPress(sym, mod, text);
    return m_omnibox.handleKeyPress(sym, mod, text);
}

} // namespace Blueprint::UI
