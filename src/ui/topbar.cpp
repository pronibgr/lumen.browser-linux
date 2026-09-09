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

CompactTopbar::CompactTopbar() {}

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

    // Smooth lerp for certificate banner alpha
    float certTarget = m_certBannerOpen ? 1.0f : 0.0f;
    float certSpeed = 1.0f - std::exp(-24.0f * dt);
    m_certBannerAlpha += (certTarget - m_certBannerAlpha) * certSpeed;

    // Hover timer for certificate explanation tooltip (2 seconds threshold)
    if (m_certBannerOpen && m_hoveredCertTitle) {
        m_certTitleHoverTimer += dt;
    } else {
        m_certTitleHoverTimer = 0.0;
    }

    // Smooth lerp for clear data modal alpha
    float modalTarget = m_clearModalOpen ? 1.0f : 0.0f;
    float modalSpeed = 1.0f - std::exp(-22.0f * dt);
    m_clearModalAlpha += (modalTarget - m_clearModalAlpha) * modalSpeed;

    // Hover timer for clear data info tooltip (1.5 seconds threshold)
    if (m_clearModalOpen && m_hoveredClearInfo) {
        m_clearInfoHoverTimer += dt;
    } else {
        m_clearInfoHoverTimer = 0.0;
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
           (std::abs(m_certBannerAlpha - certTarget) > 0.002f) ||
           (std::abs(m_clearModalAlpha - modalTarget) > 0.002f) ||
           (m_certTitleHoverTimer > 0.0 && m_certTitleHoverTimer < 2.2) ||
           (m_clearInfoHoverTimer > 0.0 && m_clearInfoHoverTimer < 1.7);
}

void CompactTopbar::updateLayout(double w) {
    m_cachedW = w;
    // Row 2: nav buttons on left, action buttons on right
    double navW = 3 * 32.0 + 4;
    double actW = 2 * 32.0 + 4;
    m_omniboxX = navW + 6;
    m_omniboxW = w - navW - actW - 16;
    m_omniboxY = Theme::ROW1_HEIGHT + (Theme::ROW2_HEIGHT - Theme::OMNIBOX_HEIGHT) / 2.0;

    m_lockX = m_omniboxX + 4.0;
    m_lockY = m_omniboxY + (Theme::OMNIBOX_HEIGHT - 24.0) / 2.0;
    m_lockW = 24.0;
    m_lockH = 24.0;
}

// ─────────────────────────── Nav button icons ──────────────────────────────
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
        // Back: chevron left
        cairo_new_path(cr);
        cairo_move_to(cr, cx + 4.5, cy - 5.5);
        cairo_line_to(cr, cx - 3.5, cy);
        cairo_line_to(cr, cx + 4.5, cy + 5.5);
        cairo_stroke(cr);
    } else if (btnIdx == 1) {
        // Forward: chevron right
        cairo_new_path(cr);
        cairo_move_to(cr, cx - 4.5, cy - 5.5);
        cairo_line_to(cr, cx + 3.5, cy);
        cairo_line_to(cr, cx - 4.5, cy + 5.5);
        cairo_stroke(cr);
    } else if (btnIdx == 2) {
        // Modern crisp reload icon (Chromium / Lucide style)
        cairo_set_line_width(cr, 1.6);
        // Arrow head (corner at top-right pointing counter-clockwise / inward)
        cairo_new_path(cr);
        cairo_move_to(cr, cx + 4.6, cy - 5.2);
        cairo_line_to(cr, cx + 4.6, cy - 1.2);
        cairo_line_to(cr, cx + 0.6, cy - 1.2);
        cairo_stroke(cr);
        // Smooth 280-degree circular arc
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, 4.6, -0.25, 3.85);
        cairo_stroke(cr);
    }
}

void CompactTopbar::drawActionBtn(cairo_t* cr, double cx, double cy, int btnIdx, bool active) {
    bool hov = (m_hoveredNav == btnIdx);
    double r = 13.0;

    if (hov || active) {
        cairo_arc(cr, cx, cy, r, 0, 2*M_PI);
        sc(cr, hov ? Theme::BG_ACTIVE : Theme::BG_SUBTLE, active ? 0.6f : 1.0f);
        cairo_fill(cr);
    }

    sc(cr, (active || hov) ? Theme::ACCENT_CALM : Theme::TEXT_MUTED);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    if (btnIdx == 3) {
        // Settings: slider icon
        cairo_set_line_width(cr, 1.6);
        for (int i = 0; i < 3; ++i) {
            double ly = cy - 5 + i * 5;
            cairo_new_path(cr);
            cairo_move_to(cr, cx - 7, ly);
            cairo_line_to(cr, cx + 7, ly);
            cairo_stroke(cr);
            double tx = (i == 0) ? cx - 2 : (i == 1) ? cx + 2 : cx;
            cairo_arc(cr, tx, ly, 2.5, 0, 2*M_PI);
            sc(cr, (active || hov) ? Theme::ACCENT_CALM : Theme::TEXT_MUTED);
            cairo_fill(cr);
            sc(cr, Theme::BG_SURFACE);
            cairo_arc(cr, tx, ly, 1.2, 0, 2*M_PI);
            cairo_fill(cr);
            sc(cr, (active || hov) ? Theme::ACCENT_CALM : Theme::TEXT_MUTED);
        }
    }
}

// ─────────────────────────── Lock Icon ─────────────────────────────────────
void CompactTopbar::drawLockIcon(cairo_t* cr, double x, double y) {
    double w = m_lockW, h = m_lockH;
    double cx = x + w / 2.0, cy = y + h / 2.0;

    // Hover highlight
    if (m_hoveredLock || m_certBannerOpen) {
        rr(cr, x, y, w, h, 6.0);
        sc(cr, Theme::BG_ACTIVE);
        cairo_fill(cr);
    }

    bool isInternal = (m_currentUrl.empty() || m_currentUrl == "lampa://newtab" || m_currentUrl == "blueprint://newtab" || m_currentUrl == "about:blank");

    Theme::Color lockCol;
    if (isInternal) {
        lockCol = Theme::TEXT_MUTED;
    } else if (m_tlsInfo.isHttps && m_tlsInfo.isValid) {
        lockCol = Theme::Color{0.12f, 0.78f, 0.45f, 1.0f}; // Emerald Green
    } else {
        lockCol = Theme::Color{0.92f, 0.25f, 0.25f, 1.0f}; // Crimson Red
    }

    sc(cr, lockCol);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    // Padlock shackle (upper arch)
    cairo_set_line_width(cr, 1.6);
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy - 2.5, 3.6, M_PI, 2 * M_PI);
    cairo_stroke(cr);

    // Padlock body (lower box)
    rr(cr, cx - 5.5, cy - 1.5, 11.0, 8.5, 2.0);
    sc(cr, lockCol);
    cairo_fill(cr);

    // Padlock keyhole dot
    sc(cr, Theme::BG_SURFACE);
    cairo_arc(cr, cx, cy + 2.2, 1.2, 0, 2 * M_PI);
    cairo_fill(cr);
}

// ─────────────────────────── Row 1: Tabs ──────────────────────────────────
void CompactTopbar::drawRow1(cairo_t* cr, double w) {
    double rowH = Theme::ROW1_HEIGHT;

    // Background
    cairo_rectangle(cr, 0, 0, w, rowH);
    sc(cr, Theme::BG_SURFACE);
    cairo_fill(cr);

    // Nav buttons: 3 × 32px
    double navCY = rowH / 2.0;
    drawNavBtn(cr, 18,  navCY, 0, m_canGoBack);
    drawNavBtn(cr, 50,  navCY, 1, m_canGoForward);
    drawNavBtn(cr, 82,  navCY, 2, m_canReload);

    // Tab strip: between nav buttons (96px) and settings button (36px right)
    double tabsX = 96.0;
    double actW  = 36.0;
    double tabsW = w - tabsX - actW;
    m_tabStrip.draw(cr, tabsX, 0, tabsW, rowH);

    // Action button: settings on right side
    double actCY = rowH / 2.0;
    double actCX = w - 18;
    drawActionBtn(cr, actCX, actCY, 3, m_settings.isVisible());

    // Bottom border of row1
    sc(cr, Theme::BORDER_SOFT, 0.35f);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_move_to(cr, 0, rowH - 0.5);
    cairo_line_to(cr, w, rowH - 0.5);
    cairo_stroke(cr);
}

// ─────────────────────────── Row 2: Omnibox & Lock ────────────────────────
void CompactTopbar::drawRow2(cairo_t* cr, double w) {
    double rowY = Theme::ROW1_HEIGHT;
    double rowH = Theme::ROW2_HEIGHT;

    // Background
    cairo_rectangle(cr, 0, rowY, w, rowH);
    sc(cr, Theme::BG_ABYSS);
    cairo_fill(cr);

    // Draw Omnibox with inset for the Lock icon
    m_omnibox.draw(cr, m_omniboxX, m_omniboxY, m_omniboxW, Theme::OMNIBOX_HEIGHT);

    // Draw Lock Icon inside the left side of omnibox
    drawLockIcon(cr, m_lockX, m_lockY);

    // Neon progress
    m_neonProgress.draw(cr, 0, rowY + rowH - 2.0, w);

    // Bottom border
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

// ─────────────────────────── Security & Certificate Banner ─────────────────
void CompactTopbar::drawCertBanner(cairo_t* cr, double winW, double winH) {
    (void)winW; (void)winH;
    if (m_certBannerAlpha <= 0.01f) return;

    float alpha = m_certBannerAlpha;
    double banX = m_omniboxX + 4.0;
    double banY = m_omniboxY + Theme::OMNIBOX_HEIGHT + 6.0;
    double banW = 390.0, banH = 175.0;

    cairo_push_group(cr);

    // Drop Shadow
    rr(cr, banX + 3, banY + 3, banW, banH, 10.0);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
    cairo_fill(cr);

    // Panel Background
    rr(cr, banX, banY, banW, banH, 10.0);
    sc(cr, Theme::BG_POPUP);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, 0.7f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // 1. Connection Status Row
    double iconCX = banX + 26, iconCY = banY + 24;
    bool isInternal = (m_currentUrl.empty() || m_currentUrl == "lampa://newtab" || m_currentUrl == "blueprint://newtab" || m_currentUrl == "about:blank");
    bool isSecure = m_tlsInfo.isHttps && m_tlsInfo.isValid && !isInternal;

    Theme::Color statusCol = isInternal ? Theme::TEXT_MUTED
                             : (isSecure ? Theme::Color{0.12f, 0.78f, 0.45f, 1.0f}
                             : Theme::Color{0.92f, 0.25f, 0.25f, 1.0f});

    // Icon
    cairo_arc(cr, iconCX, iconCY - 3, 4.0, M_PI, 2 * M_PI);
    sc(cr, statusCol); cairo_set_line_width(cr, 1.7); cairo_stroke(cr);
    rr(cr, iconCX - 6, iconCY - 2, 12, 9, 2);
    sc(cr, statusCol); cairo_fill(cr);

    // Title text
    const char* titleText = isInternal ? "Внутренняя страница"
                          : (isSecure ? "Ваше подключение защищено!"
                          : "Подключение не защищено!");
    PangoLayout* lTitle = makeLayout(cr, "Inter Bold 11");
    showText(cr, lTitle, titleText, banX + 46, banY + 16, Theme::TEXT_MAIN);

    // 2. Certificate status description and pill
    PangoLayout* lSub = makeLayout(cr, "Inter 9");
    const char* subText = isInternal ? "Служебный ресурс браузера:"
                        : (isSecure ? "Сертификат подтверждён:"
                        : "Статус безопасности:");
    showText(cr, lSub, subText, banX + 46, banY + 44, Theme::TEXT_MUTED);

    // Pill
    std::string pillLabel;
    Theme::Color pillTextColor;
    if (isInternal) {
        pillLabel = "Локальная страница";
        pillTextColor = Theme::TEXT_MUTED;
    } else if (isSecure) {
        pillLabel = m_tlsInfo.issuer.empty() ? "Центр сертификации" : m_tlsInfo.issuer;
        pillTextColor = Theme::Color{0.38f, 0.65f, 0.98f, 1.0f};
    } else {
        pillLabel = "Сертификат недействителен или отсутствует";
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
        cairo_set_source_rgba(cr, 0.10, 0.22, 0.46, 0.85); // Dark blue
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0.25, 0.48, 0.92, 0.65);
    } else {
        cairo_set_source_rgba(cr, 0.45, 0.12, 0.12, 0.75); // Dark red warning
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0.85, 0.25, 0.25, 0.65);
    }
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    showText(cr, lCert, pillLabel.c_str(), pillX + 9, pillY + 4.5, pillTextColor);

    // 3. Vector Divider Line
    double sepY = banY + 104.0;
    cairo_set_line_width(cr, 1.0);
    sc(cr, Theme::BORDER_SOFT, 0.45f);
    cairo_new_path(cr);
    cairo_move_to(cr, banX + 16, sepY);
    cairo_line_to(cr, banX + banW - 16, sepY);
    cairo_stroke(cr);

    // 4. "Clear Data on this site" Button
    double btnX = banX + 16, btnY = banY + 120.0;
    double btnW = banW - 32, btnH = 34.0;
    rr(cr, btnX, btnY, btnW, btnH, 6.0);

    if (!m_canClearData) {
        // Disabled button state
        sc(cr, Theme::BG_SUBTLE, 0.35f);
        cairo_fill_preserve(cr);
        sc(cr, Theme::BORDER_SOFT, 0.3f);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        // Dimmed icon
        double trX = btnX + 16, trY = btnY + btnH / 2.0;
        sc(cr, Theme::TEXT_MUTED, 0.4f);
        cairo_set_line_width(cr, 1.4);
        cairo_rectangle(cr, trX - 4, trY - 3, 8, 8);
        cairo_stroke(cr);
        cairo_move_to(cr, trX - 6, trY - 4); cairo_line_to(cr, trX + 6, trY - 4);
        cairo_stroke(cr);

        PangoLayout* lBtn = makeLayout(cr, "Inter SemiBold 10");
        showText(cr, lBtn, "Очистить данные (недоступно для страницы)", btnX + 32, btnY + 9, Theme::TEXT_MUTED, 0.45f);
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

        // Trash / clear icon
        double trX = btnX + 16, trY = btnY + btnH / 2.0;
        sc(cr, Theme::ACCENT_CALM);
        cairo_set_line_width(cr, 1.4);
        cairo_rectangle(cr, trX - 4, trY - 3, 8, 8);
        cairo_stroke(cr);
        cairo_move_to(cr, trX - 6, trY - 4); cairo_line_to(cr, trX + 6, trY - 4);
        cairo_stroke(cr);

        PangoLayout* lBtn = makeLayout(cr, "Inter SemiBold 10");
        showText(cr, lBtn, "Очистить мои данные на этом сайте", btnX + 32, btnY + 9, Theme::TEXT_MAIN);
        g_object_unref(lBtn);
    }

    // 5. Tooltip on Connection Status (shown after >= 1.5s hover)
    if (m_certTitleHoverTimer >= 1.5) {
        const char* tipText = isSecure
            ? "Все передаваемые данные (пароли, номера карт, сообщения) шифруются протоколом TLS и недоступны третьим лицам."
            : "Передаваемые данные не защищены сквозным шифрованием и могут быть перехвачены злоумышленниками в сети.";
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

// ─────────────────────────── Clear Data Modal Dialog ───────────────────────
void CompactTopbar::drawClearDataModal(cairo_t* cr, double winW, double winH) {
    if (m_clearModalAlpha <= 0.01f) return;

    float alpha = m_clearModalAlpha;

    // 1. Fullscreen Darkened Backdrop
    cairo_set_source_rgba(cr, 0, 0, 0, 0.65 * alpha);
    cairo_rectangle(cr, 0, 0, winW, winH);
    cairo_fill(cr);

    // 2. Centered Modal Box
    double mw = 480.0, mh = 310.0;
    double mx = (winW - mw) / 2.0, my = (winH - mh) / 2.0;

    cairo_push_group(cr);

    // Shadow
    rr(cr, mx + 6, my + 6, mw, mh, 14.0);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    cairo_fill(cr);

    // Box Surface
    rr(cr, mx, my, mw, mh, 14.0);
    sc(cr, Theme::BG_SURFACE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, 0.7f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // Header with warning badge
    rr(cr, mx + 20, my + 18, 32, 32, 8.0);
    sc(cr, Theme::Color{0.95f, 0.25f, 0.25f, 0.15f});
    cairo_fill(cr);

    PangoLayout* lWarn = makeLayout(cr, "Inter Bold 14");
    showText(cr, lWarn, "!", mx + 32, my + 22, Theme::Color{0.95f, 0.35f, 0.35f, 1.0f});

    PangoLayout* lTitle = makeLayout(cr, "Inter Bold 13");
    showText(cr, lTitle, "Удаление данных", mx + 62, my + 18, Theme::TEXT_MAIN);

    std::string siteHost = m_tlsInfo.subject.empty() ? "текущем ресурсе" : m_tlsInfo.subject;
    PangoLayout* lHost = makeLayout(cr, "Inter 10");
    std::string hostStr = "Сайт: " + siteHost;
    showText(cr, lHost, hostStr.c_str(), mx + 62, my + 36, Theme::TEXT_MUTED);

    // Inset Data Details Card
    double cardX = mx + 20, cardY = my + 64;
    double cardW = mw - 40, cardH = 112.0;
    rr(cr, cardX, cardY, cardW, cardH, 8.0);
    sc(cr, Theme::BG_SUBTLE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, 0.4f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // Real data size monitoring
    std::string sizeStr = m_canClearData ? formatBytes(m_siteDataBytes) : "0 KB";
    PangoLayout* lSize = makeLayout(cr, "Inter Bold 12");
    showText(cr, lSize, ("Размер: " + sizeStr).c_str(), cardX + 16, cardY + 12, Theme::ACCENT_CALM);

    PangoLayout* lItem = makeLayout(cr, "Inter 10");
    if (m_canClearData) {
        showText(cr, lItem, "• Файлы cookie: активные сессионные куки", cardX + 16, cardY + 36, Theme::TEXT_MAIN);
        showText(cr, lItem, "• Локальное хранилище: LocalStorage и IndexedDB", cardX + 16, cardY + 56, Theme::TEXT_MUTED);
        showText(cr, lItem, "• Кэшированные ресурсы: Изображения, скрипты и стили", cardX + 16, cardY + 76, Theme::TEXT_MUTED);
    } else {
        showText(cr, lItem, "• Локальные данные отсутствуют", cardX + 16, cardY + 36, Theme::TEXT_MUTED);
        showText(cr, lItem, "• Файлы cookie отсутствуют", cardX + 16, cardY + 56, Theme::TEXT_MUTED);
        showText(cr, lItem, "• Кэшированные ресурсы отсутствуют", cardX + 16, cardY + 76, Theme::TEXT_MUTED);
    }

    // Warning text
    PangoLayout* lWarnText = makeLayout(cr, "Inter SemiBold 9.5", static_cast<int>(cardW));
    const char* warnStr = m_canClearData
        ? "Вы действительно хотите удалить ваши данные на этом сайте? ВАЖНО: Это действие необратимо."
        : "Удаление недоступно: данная страница является внутренней или не была загружена.";
    pango_layout_set_text(lWarnText, warnStr, -1);
    showText(cr, lWarnText, warnStr, mx + 20, my + 192,
             m_canClearData ? Theme::Color{0.95f, 0.35f, 0.35f, 1.0f} : Theme::TEXT_MUTED);

    // Vector divider
    sc(cr, Theme::BORDER_SOFT, 0.4f);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, mx + 20, my + 242);
    cairo_line_to(cr, mx + mw - 20, my + 242);
    cairo_stroke(cr);

    // [i] Info icon
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

    // Buttons: "Удалить" (Red) and "Отмена" (Blue)
    double btnDelX = mx + mw - 195, btnDelY = my + mh - 46;
    double btnDelW = 85.0, btnDelH = 30.0;
    rr(cr, btnDelX, btnDelY, btnDelW, btnDelH, 6.0);
    if (!m_canClearData) {
        cairo_set_source_rgba(cr, 0.35, 0.15, 0.15, 0.45); // Disabled inactive
    } else if (m_hoveredClearConfirm) {
        cairo_set_source_rgba(cr, 0.75, 0.12, 0.12, 1.0); // Darker red on hover
    } else {
        cairo_set_source_rgba(cr, 0.88, 0.16, 0.16, 1.0); // Red
    }
    cairo_fill(cr);

    // Mathematically centered "Удалить" text
    PangoLayout* lDel = makeLayout(cr, "Inter Bold 10");
    pango_layout_set_text(lDel, "Удалить", -1);
    int dw = 0, dh = 0;
    pango_layout_get_pixel_size(lDel, &dw, &dh);
    double txDel = btnDelX + (btnDelW - dw) / 2.0;
    double tyDel = btnDelY + (btnDelH - dh) / 2.0;
    showText(cr, lDel, "Удалить", txDel, tyDel, Theme::Color{1, 1, 1, m_canClearData ? 1.0f : 0.4f});

    // Mathematically centered "Отмена" text
    double btnCanX = mx + mw - 95, btnCanY = my + mh - 46;
    double btnCanW = 75.0, btnCanH = 30.0;
    rr(cr, btnCanX, btnCanY, btnCanW, btnCanH, 6.0);
    if (m_hoveredClearCancel) {
        cairo_set_source_rgba(cr, 0.12, 0.35, 0.85, 1.0); // Blue hover
    } else {
        cairo_set_source_rgba(cr, 0.15, 0.42, 0.95, 1.0); // Blue
    }
    cairo_fill(cr);

    PangoLayout* lCan = makeLayout(cr, "Inter Bold 10");
    pango_layout_set_text(lCan, "Отмена", -1);
    int cw_can = 0, ch_can = 0;
    pango_layout_get_pixel_size(lCan, &cw_can, &ch_can);
    double txCan = btnCanX + (btnCanW - cw_can) / 2.0;
    double tyCan = btnCanY + (btnCanH - ch_can) / 2.0;
    showText(cr, lCan, "Отмена", txCan, tyCan, Theme::Color{1, 1, 1, 1});

    // Tooltip for [i] info icon
    if (m_clearInfoHoverTimer >= 1.5) {
        const char* tipInfo = "Удаляемые данные могут содержать сохранённые сессии входа, пароли, персонализированные настройки сайта и временные файлы.";
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
    // 1. Omnibox dropdown (below row 2)
    m_omnibox.drawPopup(cr, m_omniboxX, m_omniboxY + Theme::OMNIBOX_HEIGHT + 4, m_omniboxW);

    // 2. Security & Certificate Banner
    drawCertBanner(cr, winW, winH);

    // 3. Clear Data Modal Dialog
    drawClearDataModal(cr, winW, winH);

    // 4. Settings Panel
    m_settings.draw(cr, winW, winH);
}

bool CompactTopbar::handleMouseMove(double mx, double my) {
    if (m_settings.isVisible()) {
        return m_settings.handleMouseMove(mx, my);
    }

    // Modal dialog hover handling
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

    // Certificate banner hover handling
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

    int old = m_hoveredNav;
    m_hoveredNav = -1;

    double r = 14.0;
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
    double actCY = double(Theme::ROW1_HEIGHT) / 2.0;
    double actCX = m_cachedW - 18;
    if (m_cachedW > 0) {
        if (std::hypot(mx - actCX, my - actCY) <= r) m_hoveredNav = 3;
    }

    // Lock icon hover
    bool oldLock = m_hoveredLock;
    m_hoveredLock = (mx >= m_lockX && mx <= m_lockX + m_lockW &&
                     my >= m_lockY && my <= m_lockY + m_lockH);

    bool ch = (old != m_hoveredNav || oldLock != m_hoveredLock);
    if (m_tabStrip.handleMouseMove(mx, my)) ch = true;
    if (m_omnibox.handleMouseMove(mx, my))  ch = true;
    return ch;
}

bool CompactTopbar::handleMouseDown(double mx, double my) {
    if (m_settings.isVisible()) {
        return m_settings.handleMouseDown(mx, my);
    }

    // Modal dialog click handling
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
        // Click outside dialog box closes modal
        double mw = 480.0, mh = 310.0;
        double winW = (m_cachedW > 0) ? m_cachedW : 1280.0;
        double winH = 800.0;
        double bx = (winW - mw) / 2.0, by = (winH - mh) / 2.0;
        if (mx < bx || mx > bx + mw || my < by || my > by + mh) {
            m_clearModalOpen = false;
            return true;
        }
        return true; // absorb click inside modal
    }

    // Certificate banner click handling
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
            // Clicked outside banner
            m_certBannerOpen = false;
            // If clicked on lock icon, toggle it
            if (mx >= m_lockX && mx <= m_lockX + m_lockW && my >= m_lockY && my <= m_lockY + m_lockH) {
                return true;
            }
        } else {
            return true; // absorb click inside banner
        }
    }

    // Lock button click
    if (mx >= m_lockX && mx <= m_lockX + m_lockW && my >= m_lockY && my <= m_lockY + m_lockH) {
        m_certBannerOpen = !m_certBannerOpen;
        return true;
    }

    if (m_hoveredNav == 0 && m_canGoBack    && m_onBack)    { m_onBack();    return true; }
    if (m_hoveredNav == 1 && m_canGoForward && m_onForward) { m_onForward(); return true; }
    if (m_hoveredNav == 2 && m_canReload    && m_onReload)  { m_onReload();  return true; }
    if (m_hoveredNav == 3) { m_settings.toggle(); return true; }

    // Row 1: Nav buttons, tab strip, and settings button
    if (my <= Theme::ROW1_HEIGHT) {
        if (m_tabStrip.handleMouseDown(mx, my, 1)) {
            m_omnibox.setFocused(false);
            return true;
        }
        return false;
    }

    // Row 2: Omnibox (single click focus and text editing)
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

bool CompactTopbar::handleKeyPress(uint32_t sym, uint16_t mod, const char* text) {
    if (m_clearModalOpen) {
        m_clearModalOpen = false;
        return true;
    }
    if (m_certBannerOpen) {
        m_certBannerOpen = false;
        return true;
    }
    if (m_settings.isVisible()) return m_settings.handleKeyPress(sym, text);
    (void)mod;
    return m_omnibox.handleKeyPress(sym, mod, text);
}

} // namespace Blueprint::UI
