#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "ui/settings_panel.hpp"
#include "theme/colors.hpp"
#include "storage/database.hpp"
#include <gtk/gtk.h>
#include <SDL2/SDL.h>
#include <pango/pangocairo.h>
#include <algorithm>
#include <cstring>
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

// bezier ease for smooth animations
static float easeOut(float t) {
    t = 1.f - t;
    return 1.f - t*t*t;
}

// check if codepoint is cyrillic
static bool isCyrillicCodePoint(uint32_t cp) {
    return (cp >= 0x0400 && cp <= 0x04FF) || // cyrillic
           (cp >= 0x0500 && cp <= 0x052F) || // cyrillic supplement
           (cp >= 0x2DE0 && cp <= 0x2DFF) || // cyrillic extended-a
           (cp >= 0xA640 && cp <= 0xA69F) || // cyrillic extended-b
           (cp >= 0x1C80 && cp <= 0x1C8F);   // cyrillic extended-c
}

static bool stringHasCyrillic(const char* str) {
    if (!str) return false;
    const char* p = str;
    while (*p) {
        gunichar c = g_utf8_get_char(p);
        if (isCyrillicCodePoint(c)) return true;
        p = g_utf8_next_char(p);
    }
    return false;
}
} // namespace

SettingsPanel::SettingsPanel()
    : m_openAnim(260.f, Engine::CubicBezier(0.16f, 1.f, 0.3f, 1.f))
    , m_sectAnim(180.f, Engine::CubicBezier(0.16f, 1.f, 0.3f, 1.f))
{
    // top 5 worldwide non-cis search engines
    m_settings.searchEngines = {
        { "Google",     "https://www.google.com/search?q=%s", false },
        { "DuckDuckGo", "https://duckduckgo.com/?q=%s",       false },
        { "Bing",       "https://www.bing.com/search?q=%s",   false },
        { "Yahoo!",     "https://search.yahoo.com/search?p=%s", false },
        { "Ecosia",     "https://www.ecosia.org/search?q=%s", false }
    };
    loadSavedEngines();
    loadSavedUserAgents();
}

void SettingsPanel::loadSavedUserAgents() {
    m_settings.userAgents = {
        { "Chrome 131 (Linux)",   "Linux",   "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36" },
        { "Chrome 131 (Windows)", "Windows", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36" },
        { "Firefox 133 (Linux)",   "Linux",   "Mozilla/5.0 (X11; Linux x86_64; rv:133.0) Gecko/20100101 Firefox/133.0" },
        { "Firefox 133 (Windows)", "Windows", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:133.0) Gecko/20100101 Firefox/133.0" },
        { "Safari 18.1 (macOS)",   "macOS",   "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.1 Safari/605.1.15" },
        { "Edge 131 (Windows)",    "Windows", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36 Edg/131.0.0.0" },
        { "Lumen Browser (Default)", "WebKit", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Safari/605.1.15 lumen browser/1.0" }
    };

    std::string savedUa = Storage::Database::instance().getSetting("user_agent_active_value", "");
    std::string savedIdxStr = Storage::Database::instance().getSetting("user_agent_active_index", "0");
    int savedIdx = 0;
    try { savedIdx = std::stoi(savedIdxStr); } catch (...) {}

    m_settings.activeUserAgentIndex = 0; // default to Chrome 131 Linux
    if (!savedUa.empty()) {
        bool found = false;
        for (size_t i = 0; i < m_settings.userAgents.size(); ++i) {
            if (m_settings.userAgents[i].userAgent == savedUa) {
                m_settings.activeUserAgentIndex = static_cast<int>(i);
                found = true;
                break;
            }
        }
        if (!found && savedIdx >= 0 && savedIdx < static_cast<int>(m_settings.userAgents.size())) {
            m_settings.activeUserAgentIndex = savedIdx;
        }
    } else if (savedIdx >= 0 && savedIdx < static_cast<int>(m_settings.userAgents.size())) {
        m_settings.activeUserAgentIndex = savedIdx;
    }
    saveActiveUserAgent();
}

void SettingsPanel::saveActiveUserAgent() {
    if (m_settings.activeUserAgentIndex >= 0 &&
        m_settings.activeUserAgentIndex < static_cast<int>(m_settings.userAgents.size())) {
        const auto& item = m_settings.userAgents[m_settings.activeUserAgentIndex];
        Storage::Database::instance().setSetting("user_agent_active_index", std::to_string(m_settings.activeUserAgentIndex));
        Storage::Database::instance().setSetting("user_agent_active_value", item.userAgent);
        Storage::Database::instance().setSetting("user_agent_active_name", item.name);
    }
}

void SettingsPanel::selectUserAgent(int index) {
    if (index >= 0 && index < static_cast<int>(m_settings.userAgents.size())) {
        m_settings.activeUserAgentIndex = index;
        saveActiveUserAgent();
        if (m_onUserAgentChanged) {
            m_onUserAgentChanged(m_settings.getActiveUserAgent());
        }
    }
}

void SettingsPanel::loadSavedEngines() {
    // load saved custom search engines from db
    std::string countStr = Storage::Database::instance().getSetting("search_custom_count", "0");
    int count = 0;
    try { count = std::stoi(countStr); } catch (...) {}
    for (int i = 0; i < count; ++i) {
        std::string name = Storage::Database::instance().getSetting("search_custom_name_" + std::to_string(i), "");
        std::string tmpl = Storage::Database::instance().getSetting("search_custom_url_" + std::to_string(i), "");
        if (!name.empty() && !tmpl.empty()) {
            m_settings.searchEngines.push_back({ name, tmpl, true });
        }
    }

    std::string activeName = Storage::Database::instance().getSetting("search_engine_active_name", "");
    std::string activeUrl  = Storage::Database::instance().getSetting("search_engine_active_url", "");
    if (activeName.empty()) {
        activeName = Storage::Database::instance().getSetting("search_engine_active", "DuckDuckGo");
    }

    m_settings.activeSearchEngineIndex = 1; // default to DuckDuckGo
    for (size_t i = 0; i < m_settings.searchEngines.size(); ++i) {
        if (!activeUrl.empty() && m_settings.searchEngines[i].urlTemplate == activeUrl) {
            m_settings.activeSearchEngineIndex = static_cast<int>(i);
            break;
        } else if (m_settings.searchEngines[i].name == activeName) {
            m_settings.activeSearchEngineIndex = static_cast<int>(i);
            break;
        }
    }
    saveActiveEngine();
}

void SettingsPanel::saveActiveEngine() {
    if (m_settings.activeSearchEngineIndex >= 0 &&
        m_settings.activeSearchEngineIndex < static_cast<int>(m_settings.searchEngines.size())) {
        const auto& eng = m_settings.searchEngines[m_settings.activeSearchEngineIndex];
        Storage::Database::instance().setSetting("search_engine_active_name", eng.name);
        Storage::Database::instance().setSetting("search_engine_active_url", eng.urlTemplate);
        Storage::Database::instance().setSetting("search_engine_template", eng.urlTemplate);
        Storage::Database::instance().setSetting("search_engine_active", eng.name);
    }
}

void SettingsPanel::saveCustomEnginesToDb() {
    int customCount = 0;
    for (const auto& eng : m_settings.searchEngines) {
        if (eng.isCustom) {
            Storage::Database::instance().setSetting("search_custom_name_" + std::to_string(customCount), eng.name);
            Storage::Database::instance().setSetting("search_custom_url_" + std::to_string(customCount), eng.urlTemplate);
            customCount++;
        }
    }
    Storage::Database::instance().setSetting("search_custom_count", std::to_string(customCount));
    // clear excess slots from db
    for (int i = customCount; i < customCount + 20; ++i) {
        Storage::Database::instance().setSetting("search_custom_name_" + std::to_string(i), "");
        Storage::Database::instance().setSetting("search_custom_url_" + std::to_string(i), "");
    }
}

bool SettingsPanel::isCreateSearchValid() const {
    const auto& s = m_createSearchInput;
    // rule 1: must start with https://
    if (s.rfind("https://", 0) != 0) return false;
    // rule 2: must contain %s placeholder
    if (s.find("%s") == std::string::npos) return false;
    // rule 3: no whitespace
    if (s.find(' ') != std::string::npos) return false;
    if (s.length() < 12 || s.length() > 2048) return false;

    // rule 4: no cyrillic characters allowed in search engine url
    if (stringHasCyrillic(s.c_str())) return false;

    // host verification: host must contain at least one dot
    std::string after = s.substr(8);
    size_t slash = after.find_first_of("/?");
    std::string host = (slash == std::string::npos) ? after : after.substr(0, slash);
    if (host.empty() || host.find('.') == std::string::npos) return false;
    if (host.front() == '.' || host.back() == '.') return false;

    // prevent control chars or unescaped quotes
    for (char c : s) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 33 || c == '"' || c == '<' || c == '>' || c == '`') return false;
    }
    return true;
}

void SettingsPanel::commitCustomSearchEngine() {
    if (!isCreateSearchValid()) return;
    std::string url = m_createSearchInput;

    // extract clean engine name from host
    std::string h = url.substr(8);
    size_t slash = h.find_first_of("/?");
    if (slash != std::string::npos) h = h.substr(0, slash);
    if (h.rfind("www.", 0) == 0) h = h.substr(4);
    if (h.rfind("search.", 0) == 0) h = h.substr(7);

    std::string name = h;
    size_t dot = name.find('.');
    if (dot != std::string::npos && dot > 0) {
        name = name.substr(0, dot);
    }
    if (!name.empty()) {
        name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    } else {
        name = "Custom Search";
    }

    m_settings.searchEngines.push_back({ name, url, true });
    m_settings.activeSearchEngineIndex = static_cast<int>(m_settings.searchEngines.size() - 1);

    saveCustomEnginesToDb();
    saveActiveEngine();

    m_createSearchModalOpen = false;
    m_createSearchInput.clear();
    m_createSearchCursor = 0;
}

void SettingsPanel::triggerDeleteCustomSearchEngine(int index) {
    if (index >= 0 && index < static_cast<int>(m_settings.searchEngines.size())) {
        if (m_settings.searchEngines[index].isCustom) {
            m_deleteTargetEngineIdx = index;
            m_deleteConfirmModalOpen = true;
        }
    }
}

void SettingsPanel::deleteCustomSearchEngine(int index) {
    if (index < 0 || index >= static_cast<int>(m_settings.searchEngines.size())) return;
    if (!m_settings.searchEngines[index].isCustom) return; // built-ins cannot be deleted

    bool wasActive = (m_settings.activeSearchEngineIndex == index);
    m_settings.searchEngines.erase(m_settings.searchEngines.begin() + index);

    if (wasActive) {
        m_settings.activeSearchEngineIndex = 1; // reset to DuckDuckGo
    } else if (m_settings.activeSearchEngineIndex > index) {
        m_settings.activeSearchEngineIndex--;
    }

    saveCustomEnginesToDb();
    saveActiveEngine();

    m_deleteConfirmModalOpen = false;
    m_deleteTargetEngineIdx = -1;
}

void SettingsPanel::toggle() {
    m_wantOpen = !m_wantOpen;
    if (m_wantOpen) m_openAnim.playForward();
    else            m_openAnim.playReverse();
}

void SettingsPanel::setVisible(bool v) {
    if (v == m_wantOpen) return;
    m_wantOpen = v;
    if (v) {
        m_openAnim.playForward();
    } else {
        m_searchDropdownOpen = false;
        m_searchEditMode = false;
        m_uaDropdownOpen = false;
        m_openAnim.playReverse();
    }
}

bool SettingsPanel::isVisible() const {
    return m_openAnim.value() > 0.001f;
}

bool SettingsPanel::wantsRedraw() const {
    bool slidersAnimating = false;
    float targets[5] = {
        m_settings.anim.tabSlide,
        m_settings.anim.tabClose,
        m_settings.anim.settingsOpen,
        m_settings.anim.sectionSwitch,
        m_settings.anim.reloadSpin
    };
    for (int i = 0; i < 5; ++i) {
        if (std::abs(m_sliderVisual[i] - targets[i]) > 0.001f) {
            slidersAnimating = true;
            break;
        }
    }
    bool themeCardsHovering = false;
    for (int i = 0; i < 16; ++i) {
        float hTarget = (m_hoveredThemeIdx == i) ? 1.0f : 0.0f;
        if (std::abs(m_themeHover[i] - hTarget) > 0.002f) {
            themeCardsHovering = true;
            break;
        }
    }
    bool appearanceScrolling = (std::abs(m_appearanceScrollY - m_appearanceTargetScrollY) > 0.05f);
    float dropTarget = m_searchDropdownOpen ? 1.0f : 0.0f;
    float uaDropTarget = m_uaDropdownOpen ? 1.0f : 0.0f;
    float modalTarget = m_createSearchModalOpen ? 1.0f : 0.0f;
    float delModalTarget = m_deleteConfirmModalOpen ? 1.0f : 0.0f;
    float editTarget = m_searchEditMode ? 1.0f : 0.0f;
    float lumenTarget = m_lumenThresholdModalOpen ? 1.0f : 0.0f;
    return m_openAnim.isRunning() || m_sectAnim.isRunning() || slidersAnimating || themeCardsHovering || appearanceScrolling ||
           Theme::ThemeManager::instance().wantsRedraw() ||
           (std::abs(m_lumenThresholdModalAlpha - lumenTarget) > 0.002f) ||
           (std::abs(m_searchDropdownAlpha - dropTarget) > 0.002f) ||
           (std::abs(m_uaDropdownAlpha - uaDropTarget) > 0.002f) ||
           (std::abs(m_createSearchModalAlpha - modalTarget) > 0.002f) ||
           (std::abs(m_deleteConfirmModalAlpha - delModalTarget) > 0.002f) ||
           (std::abs(m_searchEditModeAlpha - editTarget) > 0.002f) ||
           std::abs(m_sidebarCursorY - static_cast<float>(m_section)) > 0.002f ||
           std::abs(m_toggleAnim - (m_settings.anim.enabled ? 1.0f : 0.0f)) > 0.002f;
}

void SettingsPanel::update(float dt) {
    // update theme engine smooth transition
    Theme::ThemeManager::instance().update(dt);

    // update open animation speed from user settings
    float openDur = 260.f / std::max(0.1f, m_settings.anim.settingsOpen);
    m_openAnim.setDuration(openDur);

    float sectDur = 180.f / std::max(0.1f, m_settings.anim.sectionSwitch);
    m_sectAnim.setDuration(sectDur);

    m_openAnim.update();
    m_sectAnim.update();
    m_openProg  = m_openAnim.value();
    m_sectionProg = m_sectAnim.value();

    // smooth sidebar cursor glide (framerate-independent exponential decay)
    float targetY = static_cast<float>(m_section);
    float cursorFactor = 1.0f - std::exp(-18.0f * dt);
    m_sidebarCursorY += (targetY - m_sidebarCursorY) * cursorFactor;

    // smooth toggle pill switch
    float targetToggle = m_settings.anim.enabled ? 1.0f : 0.0f;
    float toggleFactor = 1.0f - std::exp(-20.0f * dt);
    m_toggleAnim += (targetToggle - m_toggleAnim) * toggleFactor;

    // init visual slider positions on first launch
    if (!m_sliderVisualInit) {
        m_sliderVisual[0] = m_settings.anim.tabSlide;
        m_sliderVisual[1] = m_settings.anim.tabClose;
        m_sliderVisual[2] = m_settings.anim.settingsOpen;
        m_sliderVisual[3] = m_settings.anim.sectionSwitch;
        m_sliderVisual[4] = m_settings.anim.reloadSpin;
        m_sliderVisualInit = true;
    }

    // smooth slider thumb glide animation
    float* vals[] = {
        &m_settings.anim.tabSlide,
        &m_settings.anim.tabClose,
        &m_settings.anim.settingsOpen,
        &m_settings.anim.sectionSwitch,
        &m_settings.anim.reloadSpin,
    };
    for (int i = 0; i < 5; ++i) {
        float target = *vals[i];
        // if actively dragging mouse, follow instantly (speed 36). on click, glide smoothly (speed 14)
        float speed = (m_dragSlider == i && m_sliderIsDragging) ? 36.0f : 14.0f;
        float factor = 1.0f - std::exp(-speed * dt);
        m_sliderVisual[i] += (target - m_sliderVisual[i]) * factor;
        if (std::abs(target - m_sliderVisual[i]) < 0.001f) {
            m_sliderVisual[i] = target;
        }
    }

    // smooth lerp for search engine dropdown animation
    float dropTarget = m_searchDropdownOpen ? 1.0f : 0.0f;
    float dropSpeed = 1.0f - std::exp(-22.0f * dt);
    m_searchDropdownAlpha += (dropTarget - m_searchDropdownAlpha) * dropSpeed;

    // smooth lerp for user-agent dropdown animation
    float uaDropTarget = m_uaDropdownOpen ? 1.0f : 0.0f;
    float uaDropSpeed = 1.0f - std::exp(-22.0f * dt);
    m_uaDropdownAlpha += (uaDropTarget - m_uaDropdownAlpha) * uaDropSpeed;

    // smooth lerp for search engine edit mode
    float editTarget = m_searchEditMode ? 1.0f : 0.0f;
    float editSpeed = 1.0f - std::exp(-22.0f * dt);
    m_searchEditModeAlpha += (editTarget - m_searchEditModeAlpha) * editSpeed;

    // smooth lerp for create search engine modal dialog
    float modalTarget = m_createSearchModalOpen ? 1.0f : 0.0f;
    float modalSpeed = 1.0f - std::exp(-22.0f * dt);
    m_createSearchModalAlpha += (modalTarget - m_createSearchModalAlpha) * modalSpeed;

    // smooth lerp for delete search engine confirmation modal dialog
    float delTarget = m_deleteConfirmModalOpen ? 1.0f : 0.0f;
    float delSpeed = 1.0f - std::exp(-22.0f * dt);
    m_deleteConfirmModalAlpha += (delTarget - m_deleteConfirmModalAlpha) * delSpeed;

    // smooth lerp for lumen threshold modal dialog
    float lumenTarget = m_lumenThresholdModalOpen ? 1.0f : 0.0f;
    float lumenSpeed = 1.0f - std::exp(-22.0f * dt);
    m_lumenThresholdModalAlpha += (lumenTarget - m_lumenThresholdModalAlpha) * lumenSpeed;

    // smooth lerp for appearance section scrolling
    float scrollFactor = 1.0f - std::exp(-24.0f * dt);
    m_appearanceScrollY += (m_appearanceTargetScrollY - m_appearanceScrollY) * scrollFactor;
    if (std::abs(m_appearanceTargetScrollY - m_appearanceScrollY) < 0.05f) {
        m_appearanceScrollY = m_appearanceTargetScrollY;
    }

    // smooth hover animations for appearance theme cards
    const auto& palettes = Theme::ThemeManager::instance().allPalettes();
    for (size_t i = 0; i < palettes.size() && i < 16; ++i) {
        float hTarget = (m_hoveredThemeIdx == static_cast<int>(i)) ? 1.0f : 0.0f;
        m_themeHover[i] += (hTarget - m_themeHover[i]) * (1.0f - std::exp(-18.0f * dt));
    }
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

// center text precisely inside bounds
static void drawCenteredText(cairo_t* cr, double x, double y, double w, double h,
                             const char* text, const Theme::Color& col, float sz = 10.f, bool bold = false) {
    PangoLayout* l = pango_cairo_create_layout(cr);
    std::string fdStr = std::string("Inter ") + (bold ? "Bold " : "") + std::to_string(static_cast<int>(sz));
    PangoFontDescription* fd = pango_font_description_from_string(fdStr.c_str());
    pango_layout_set_font_description(l, fd);
    pango_font_description_free(fd);
    pango_layout_set_text(l, text, -1);

    int tw = 0, th = 0;
    pango_layout_get_pixel_size(l, &tw, &th);
    double tx = x + (w - tw) / 2.0;
    double ty = y + (h - th) / 2.0;

    sc(cr, col);
    cairo_move_to(cr, tx, ty);
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

    // per-type sliders with smoothly animated visual values
    struct { const char* label; float visualVal; int id; } sliders[] = {
        { "Tab switch slide (x speed)",    m_sliderVisual[0], 10 },
        { "Tab close animation (x speed)", m_sliderVisual[1], 11 },
        { "Settings panel open (x speed)", m_sliderVisual[2], 12 },
        { "Section switch (x speed)",      m_sliderVisual[3], 13 },
        { "Reload button spin (x speed)",  m_sliderVisual[4], 14 },
    };
    for (auto& s : sliders) {
        bool hov = (m_hoveredItem == s.id || m_dragSlider == (s.id - 10));
        drawSlider(cr, x, cY, w, s.visualVal, 0.25f, 3.0f, s.label, hov, s.id);
        cY += 46;
    }
    drawLabel(cr, x, cY, "1.0x = default speed  |  >1.0x = faster  |  <1.0x = slower",
              Theme::TEXT_DIM, 9.f);
}

void SettingsPanel::drawComingSoon(cairo_t* cr, double x, double y, const char* title) {
    drawLabel(cr, x, y, title, Theme::TEXT_MAIN, 13.f, true);
    drawLabel(cr, x, y + 36, "Coming soon...", Theme::TEXT_DIM, 10.f);
}

void SettingsPanel::drawThemePaletteCircle(cairo_t* cr, double cx, double cy, double radius,
                                           const Theme::Palette& pal, float hoverProgress, bool isCurrent) {
    cairo_save(cr);
    cairo_new_path(cr);

    // micro-animation scale on hover
    double r = radius * (1.0f + 0.12f * hoverProgress);

    // ambient glow for active theme or hovered theme
    if (isCurrent || hoverProgress > 0.05f) {
        float glowA = (isCurrent ? 0.40f : 0.0f) + 0.30f * hoverProgress;
        sc(cr, pal.accent, glowA);
        cairo_set_line_width(cr, 2.0);
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, r + 3.0, 0, 2 * M_PI);
        cairo_stroke(cr);
    }

    // segmented color pie collage representing palette
    // 5 outer slices: bgBase, bgSurface, border, textMuted, textPrimary
    const Theme::Color slices[5] = {
        pal.bgBase,
        pal.bgSurface,
        pal.border,
        pal.textMuted,
        pal.textPrimary
    };

    double angleStep = (2.0 * M_PI) / 5.0;
    double startAngle = -M_PI / 2.0; // 12 o'clock

    for (int s = 0; s < 5; ++s) {
        double a1 = startAngle + s * angleStep;
        double a2 = a1 + angleStep;
        sc(cr, slices[s]);
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, r, a1, a2);
        cairo_line_to(cr, cx, cy);
        cairo_close_path(cr);
        cairo_fill(cr);
    }

    // glowing inner jewel disc in theme's accent color
    double innerR = r * 0.42;
    sc(cr, pal.accent);
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, innerR, 0, 2 * M_PI);
    cairo_fill(cr);

    // separator ring between accent core and outer slices
    sc(cr, pal.border, 0.85f);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, innerR, 0, 2 * M_PI);
    cairo_stroke(cr);

    // outer boundary border
    sc(cr, isCurrent ? pal.accent : pal.border, isCurrent ? 1.0f : 0.85f);
    cairo_set_line_width(cr, isCurrent ? 1.5 : 1.1);
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    cairo_stroke(cr);

    cairo_new_path(cr);
    cairo_restore(cr);
}

static bool getThemeCardRect(size_t palIdx, double startX, double startY, double w, double scrollY,
                             double& rx, double& ry, double& rw, double& rh) {
    const auto& palettes = Theme::ThemeManager::instance().allPalettes();
    if (palIdx >= palettes.size()) return false;

    double cardH = 54.0;
    double cardGap = 10.0;
    double secHeaderH = 26.0;
    double divH = 20.0;

    int darkRank = 0, lightRank = 0;
    for (size_t i = 0; i < palIdx; ++i) {
        if (palettes[i].isDark) darkRank++;
        else lightRank++;
    }

    int totalDarkCount = 0;
    for (const auto& p : palettes) {
        if (p.isDark) totalDarkCount++;
    }

    double y = startY - scrollY;
    if (palettes[palIdx].isDark) {
        // "Dark Themes" header takes secHeaderH
        y += secHeaderH + darkRank * (cardH + cardGap);
    } else {
        // After dark themes: secHeaderH + totalDarkCount * (cardH + cardGap) + divH + secHeaderH
        y += secHeaderH + totalDarkCount * (cardH + cardGap) + divH + secHeaderH + lightRank * (cardH + cardGap);
    }

    rx = startX;
    ry = y;
    rw = w;
    rh = cardH;
    return true;
}

void SettingsPanel::drawAppearanceSection(cairo_t* cr, double x, double y, double w, double h) {
    double cY = y;
    drawLabel(cr, x, cY, "Appearance", Theme::TEXT_MAIN, 13.f, true);
    cY += 24.0;
    drawLabel(cr, x, cY, "Curated visual themes built for ocular comfort and focus.", Theme::TEXT_DIM, 9.f);
    cY += 16.0;

    // top separator line
    sc(cr, Theme::BORDER_SOFT, 0.35f);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_move_to(cr, x, cY);
    cairo_line_to(cr, x + w, cY);
    cairo_stroke(cr);
    cY += 14.0;

    // Viewport for scrolling theme list
    double viewX = x;
    double viewY = cY;
    double viewW = w;
    double viewH = (y + h) - viewY;
    if (viewH < 100.0) viewH = 100.0;

    const auto& palettes = Theme::ThemeManager::instance().allPalettes();
    int darkCount = 0, lightCount = 0;
    for (const auto& p : palettes) {
        if (p.isDark) darkCount++;
        else lightCount++;
    }

    double cardH = 54.0;
    double cardGap = 10.0;
    double secHeaderH = 26.0;
    double divH = 20.0;

    double totalContentH = secHeaderH + darkCount * (cardH + cardGap) + divH + secHeaderH + lightCount * (cardH + cardGap) + 16.0;
    m_appearanceMaxScroll = static_cast<float>(std::max(0.0, totalContentH - viewH));

    // Clamp targets
    if (m_appearanceTargetScrollY < 0.0f) m_appearanceTargetScrollY = 0.0f;
    if (m_appearanceTargetScrollY > m_appearanceMaxScroll) m_appearanceTargetScrollY = m_appearanceMaxScroll;

    // cache position for click & hover testing
    m_appearanceX = viewX;
    m_appearanceY = viewY;
    m_appearanceW = viewW;
    m_appearanceH = viewH;

    // Clip to scroll viewport
    cairo_save(cr);
    cairo_rectangle(cr, viewX - 4.0, viewY, viewW + 8.0, viewH);
    cairo_clip(cr);

    double scrollY = m_appearanceScrollY;

    // 1. "Dark Themes" header
    double darkHeaderY = viewY - scrollY;
    drawLabel(cr, viewX, darkHeaderY + 4.0, "Dark Themes", Theme::TEXT_MUTED, 10.0f, true);

    // 2. Middle divider line between dark and light themes
    double darkBlockEnd = darkHeaderY + secHeaderH + darkCount * (cardH + cardGap);
    double divY = darkBlockEnd + divH / 2.0;
    sc(cr, Theme::BORDER_SOFT, 0.35f);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_move_to(cr, viewX, divY);
    cairo_line_to(cr, viewX + viewW, divY);
    cairo_stroke(cr);

    // 3. "Light Themes" header
    double lightHeaderY = darkBlockEnd + divH;
    drawLabel(cr, viewX, lightHeaderY + 4.0, "Light Themes", Theme::TEXT_MUTED, 10.0f, true);

    // 4. Render all theme cards (full width, exactly like original design)
    for (size_t i = 0; i < palettes.size(); ++i) {
        const auto& pal = palettes[i];
        double cardX = 0, cardY = 0, cardW = 0, chH = 0;
        if (!getThemeCardRect(i, viewX, viewY, viewW, scrollY, cardX, cardY, cardW, chH)) continue;

        // Skip drawing if completely offscreen
        if (cardY + chH < viewY - 10.0 || cardY > viewY + viewH + 10.0) continue;

        bool isCurrent = (Theme::ThemeManager::instance().currentTheme() == pal.id);
        float hov = (i < 16) ? m_themeHover[i] : 0.0f;

        // card background with smooth rounded rectangle
        rr(cr, cardX, cardY, cardW, cardH, 8.0);
        float bgAlpha = isCurrent ? 0.35f : (0.15f + 0.20f * hov);
        sc(cr, isCurrent ? Theme::BG_ACTIVE : (hov > 0.01f ? Theme::BG_ACTIVE : Theme::BG_SUBTLE), bgAlpha);
        cairo_fill_preserve(cr);

        // border: accent if active, border_focus if hovered, else border_soft
        if (isCurrent) {
            sc(cr, Theme::ACCENT_CALM, 0.85f);
            cairo_set_line_width(cr, 1.4);
        } else {
            sc(cr, hov > 0.05f ? Theme::BORDER_FOCUS : Theme::BORDER_SOFT, 0.40f + 0.45f * hov);
            cairo_set_line_width(cr, 1.0);
        }
        cairo_stroke(cr);

        // radio / indicator circle on the left
        double indCx = cardX + 22.0;
        double indCy = cardY + cardH / 2.0;
        double indR = 7.0;

        if (isCurrent) {
            sc(cr, Theme::ACCENT_CALM);
            cairo_new_path(cr);
            cairo_arc(cr, indCx, indCy, indR, 0, 2 * M_PI);
            cairo_fill(cr);

            // bright center dot
            sc(cr, Theme::BG_ABYSS);
            cairo_new_path(cr);
            cairo_arc(cr, indCx, indCy, 2.6, 0, 2 * M_PI);
            cairo_fill(cr);
        } else {
            sc(cr, Theme::BORDER_SOFT, 0.75f);
            cairo_set_line_width(cr, 1.2);
            cairo_new_path(cr);
            cairo_arc(cr, indCx, indCy, indR, 0, 2 * M_PI);
            cairo_stroke(cr);
        }

        // theme title: measure dynamic width with Pango
        double textX = cardX + 38.0;
        PangoLayout* lName = pango_cairo_create_layout(cr);
        PangoFontDescription* fdName = pango_font_description_from_string("Inter Bold 11");
        pango_layout_set_font_description(lName, fdName);
        pango_font_description_free(fdName);
        pango_layout_set_text(lName, pal.displayName.c_str(), -1);
        int nameW = 0, nameH = 0;
        pango_layout_get_pixel_size(lName, &nameW, &nameH);
        sc(cr, Theme::TEXT_MAIN);
        cairo_move_to(cr, textX, cardY + 11.0);
        pango_cairo_show_layout(cr, lName);
        g_object_unref(lName);

        // refined, harmonious badge tag (dark / light)
        double tagX = textX + nameW + 9.0;
        double tagW = 48.0;
        double tagH = 15.0;
        double tagY = cardY + 11.0 + (nameH - tagH) / 2.0;

        rr(cr, tagX, tagY, tagW, tagH, 7.5);
        if (pal.isDark) {
            sc(cr, Theme::BG_ABYSS, 0.45f);
        } else {
            sc(cr, pal.accent, 0.12f);
        }
        cairo_fill_preserve(cr);
        sc(cr, Theme::BORDER_SOFT, 0.40f);
        cairo_set_line_width(cr, 0.8);
        cairo_stroke(cr);

        // micro indicator dot inside badge
        double dotCx = tagX + 7.5;
        double dotCy = tagY + tagH / 2.0;
        cairo_new_path(cr);
        sc(cr, pal.isDark ? Theme::TEXT_MUTED : pal.accent, 0.85f);
        cairo_arc(cr, dotCx, dotCy, 2.0, 0, 2 * M_PI);
        cairo_fill(cr);

        // label inside badge ("dark" / "light")
        drawCenteredText(cr, tagX + 11.0, tagY, tagW - 13.0, tagH,
                         pal.isDark ? "dark" : "light",
                         pal.isDark ? Theme::TEXT_MUTED : Theme::TEXT_MAIN, 7.5f, false);

        // subtitle / description
        drawLabel(cr, textX, cardY + 31.0, pal.description.c_str(), Theme::TEXT_MUTED, 8.5f);

        // right side: Palette Circle-Collage
        double circleCx = cardX + cardW - 34.0;
        double circleCy = cardY + cardH / 2.0;
        drawThemePaletteCircle(cr, circleCx, circleCy, 15.0, pal, hov, isCurrent);
    }

    cairo_restore(cr);

    // Draw subtle scrollbar if content exceeds viewport
    if (m_appearanceMaxScroll > 1.0f) {
        double sbW = 3.5;
        double sbX = viewX + viewW - sbW + 1.0;
        double trackH = viewH;
        double thumbH = std::max(24.0, trackH * (viewH / totalContentH));
        double thumbY = viewY + (trackH - thumbH) * (m_appearanceScrollY / m_appearanceMaxScroll);

        rr(cr, sbX, thumbY, sbW, thumbH, sbW / 2.0);
        sc(cr, Theme::TEXT_MUTED, 0.40f);
        cairo_fill(cr);
    }
}

void SettingsPanel::drawSearchSection(cairo_t* cr, double x, double y, double w) {
    double cY = y;

    drawLabel(cr, x, cY, "Search", Theme::TEXT_MAIN, 13.f, true);
    cY += 26;

    sc(cr, Theme::BORDER_SOFT, 0.3f);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_move_to(cr, x, cY); cairo_line_to(cr, x + w, cY); cairo_stroke(cr);
    cY += 16;

    drawLabel(cr, x, cY, "Default Search Engine", Theme::TEXT_MAIN, 10.5f, true);
    cY += 18;
    drawLabel(cr, x, cY, "Choose which search engine is used when querying the omnibox.", Theme::TEXT_DIM, 9.f);
    cY += 22;

    // dropdown trigger button
    double trigH = 38.0;
    m_searchTriggerX = x;
    m_searchTriggerY = cY;
    m_searchTriggerW = w;
    m_searchTriggerH = trigH;

    bool trigHover = (m_hoveredItem == 300);
    rr(cr, x, cY, w, trigH, 7.0);
    sc(cr, trigHover ? Theme::BG_ACTIVE : Theme::BG_SUBTLE);
    cairo_fill_preserve(cr);
    sc(cr, trigHover ? Theme::ACCENT_CALM : Theme::BORDER_SOFT, trigHover ? 0.75f : 0.45f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // active engine name and url preview
    std::string activeName = "DuckDuckGo";
    std::string activeUrl = "";
    if (m_settings.activeSearchEngineIndex >= 0 &&
        m_settings.activeSearchEngineIndex < static_cast<int>(m_settings.searchEngines.size())) {
        activeName = m_settings.searchEngines[m_settings.activeSearchEngineIndex].name;
        activeUrl  = m_settings.searchEngines[m_settings.activeSearchEngineIndex].urlTemplate;
    }

    drawLabel(cr, x + 14, cY + 11, activeName.c_str(), Theme::TEXT_MAIN, 10.5f, true);

    // show url hint truncated
    std::string shortUrl = activeUrl;
    if (shortUrl.length() > 36) shortUrl = shortUrl.substr(0, 33) + "...";
    drawLabel(cr, x + 125, cY + 12, shortUrl.c_str(), Theme::TEXT_DIM, 8.5f);

    // chevron icon (rotates or flips depending on open state)
    double chX = x + w - 20;
    double chY = cY + trigH / 2.0;
    sc(cr, trigHover ? Theme::TEXT_MAIN : Theme::TEXT_MUTED);
    cairo_set_line_width(cr, 1.6);
    cairo_new_path(cr);
    if (m_searchDropdownOpen) {
        cairo_move_to(cr, chX - 5, chY + 2);
        cairo_line_to(cr, chX, chY - 3);
        cairo_line_to(cr, chX + 5, chY + 2);
    } else {
        cairo_move_to(cr, chX - 5, chY - 2);
        cairo_line_to(cr, chX, chY + 3);
        cairo_line_to(cr, chX + 5, chY - 2);
    }
    cairo_stroke(cr);

    // animated dropdown list
    if (m_searchDropdownAlpha > 0.005f) {
        double itemH = 32.0;
        int nItems = static_cast<int>(m_settings.searchEngines.size());
        // in edit mode, "+ Create" item appears, smoothly scaling height
        double createBtnH = 32.0 * m_searchEditModeAlpha;
        double totalH = nItems * itemH + 6.0 + createBtnH + 34.0 + 8.0;
        double dropX = x;
        double dropY = cY + trigH + 6.0;
        double dropW = w;

        m_dropdownX = dropX;
        m_dropdownY = dropY;
        m_dropdownW = dropW;
        m_dropdownH = totalH;

        float alpha = m_searchDropdownAlpha;
        double shiftY = (1.0f - alpha) * -6.0;

        cairo_save(cr);
        cairo_translate(cr, 0, shiftY);

        // shadow
        rr(cr, dropX + 2, dropY + 3, dropW, totalH, 8.0);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.35 * alpha);
        cairo_fill(cr);

        // background card
        rr(cr, dropX, dropY, dropW, totalH, 8.0);
        sc(cr, Theme::BG_SURFACE);
        cairo_fill_preserve(cr);
        sc(cr, Theme::BORDER_SOFT, 0.75f * alpha);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        // engine items
        double curItemY = dropY + 4.0;
        for (int i = 0; i < nItems; ++i) {
            bool isHover = (m_hoveredDropdownIdx == i);
            bool isActive = (i == m_settings.activeSearchEngineIndex);
            bool isCustom = m_settings.searchEngines[i].isCustom;

            if (isHover || isActive) {
                rr(cr, dropX + 4.0, curItemY, dropW - 8.0, itemH - 2.0, 5.0);
                sc(cr, isActive ? Theme::BG_ACTIVE : Theme::BG_SUBTLE, alpha);
                cairo_fill(cr);
            }

            // in edit mode, custom engines get a red circle with a minus (-) icon
            if (isCustom && m_searchEditModeAlpha > 0.001f) {
                float minusAlpha = alpha * m_searchEditModeAlpha;
                double mcX = dropX + 16.0;
                double mcY = curItemY + (itemH - 2.0) / 2.0;
                bool minusHover = (m_hoveredDropdownIdx == 1000 + i);

                // red circle
                cairo_new_path(cr);
                cairo_arc(cr, mcX, mcY, minusHover ? 7.5 : 6.5, 0, 2 * M_PI);
                sc(cr, Theme::DANGER, minusAlpha);
                cairo_fill(cr);

                // white minus bar
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, minusAlpha);
                cairo_set_line_width(cr, 1.8);
                cairo_new_path(cr);
                cairo_move_to(cr, mcX - 3.5, mcY);
                cairo_line_to(cr, mcX + 3.5, mcY);
                cairo_stroke(cr);
            }

            // active indicator dot (shown only when not in edit mode on custom)
            if (isActive && (!isCustom || m_searchEditModeAlpha < 0.8f)) {
                float dotAlpha = alpha * (isCustom ? (1.0f - m_searchEditModeAlpha) : 1.0f);
                if (dotAlpha > 0.01f) {
                    cairo_arc(cr, dropX + 14.0, curItemY + (itemH - 2.0) / 2.0, 3.0, 0, 2 * M_PI);
                    sc(cr, Theme::ACCENT_CALM, dotAlpha);
                    cairo_fill(cr);
                }
            }

            // label with smooth slide when minus icon appears on custom engines
            double textOffset = (isCustom ? (14.0 * m_searchEditModeAlpha) : 0.0);
            drawLabel(cr, dropX + (isActive ? 26.0 : 18.0) + textOffset, curItemY + 8.0,
                      m_settings.searchEngines[i].name.c_str(),
                      isActive ? Theme::TEXT_MAIN : (isHover ? Theme::TEXT_MAIN : Theme::TEXT_MUTED),
                      9.5f, isActive);

            // small url domain on right
            std::string hint = m_settings.searchEngines[i].urlTemplate;
            if (hint.rfind("https://", 0) == 0) hint = hint.substr(8);
            size_t slash = hint.find('/');
            if (slash != std::string::npos) hint = hint.substr(0, slash);
            drawLabel(cr, dropX + dropW - 130.0, curItemY + 9.0, hint.c_str(), Theme::TEXT_DIM, 8.0f);

            curItemY += itemH;
        }

        // subtle separator
        sc(cr, Theme::BORDER_SOFT, 0.35f * alpha);
        cairo_set_line_width(cr, 1.0);
        cairo_new_path(cr);
        cairo_move_to(cr, dropX + 8.0, curItemY + 2.0);
        cairo_line_to(cr, dropX + dropW - 8.0, curItemY + 2.0);
        cairo_stroke(cr);
        curItemY += 5.0;

        // + Create item (appears only in edit mode with smooth height and alpha)
        if (m_searchEditModeAlpha > 0.005f) {
            float createAlpha = alpha * m_searchEditModeAlpha;
            cairo_save(cr);
            cairo_rectangle(cr, dropX, curItemY, dropW, 30.0 * m_searchEditModeAlpha);
            cairo_clip(cr);

            bool createHover = (m_hoveredDropdownIdx == 997);
            if (createHover) {
                rr(cr, dropX + 4.0, curItemY, dropW - 8.0, 30.0, 5.0);
                sc(cr, Theme::BG_SUBTLE, createAlpha);
                cairo_fill(cr);
            }

            // soft vector plus icon
            double plusX = dropX + 22.0;
            double plusY = curItemY + 15.0;
            sc(cr, Theme::ACCENT_CALM, createAlpha);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_set_line_width(cr, 1.8);
            cairo_new_path(cr);
            cairo_move_to(cr, plusX - 4.5, plusY);
            cairo_line_to(cr, plusX + 4.5, plusY);
            cairo_move_to(cr, plusX, plusY - 4.5);
            cairo_line_to(cr, plusX, plusY + 4.5);
            cairo_stroke(cr);

            drawLabel(cr, plusX + 12.0, curItemY + 7.0, "Create",
                      createHover ? Theme::ACCENT_CALM : Theme::TEXT_MAIN, 9.5f, true);

            cairo_restore(cr);
            curItemY += 32.0 * m_searchEditModeAlpha;
        }

        // Edit / Done item (vector pencil & paper or checkmark)
        bool editHover = (m_hoveredDropdownIdx == 998);
        if (editHover) {
            rr(cr, dropX + 4.0, curItemY, dropW - 8.0, 30.0, 5.0);
            sc(cr, Theme::BG_SUBTLE, alpha);
            cairo_fill(cr);
        }

        double editIconX = dropX + 22.0;
        double editIconY = curItemY + 15.0;

        if (m_searchEditMode) {
            // checkmark icon for Done
            sc(cr, Theme::ACCENT_CALM, alpha);
            cairo_set_line_width(cr, 1.8);
            cairo_new_path(cr);
            cairo_move_to(cr, editIconX - 4.5, editIconY);
            cairo_line_to(cr, editIconX - 1.0, editIconY + 3.5);
            cairo_line_to(cr, editIconX + 4.5, editIconY - 3.5);
            cairo_stroke(cr);

            drawLabel(cr, editIconX + 12.0, curItemY + 7.0, "Done",
                      editHover ? Theme::TEXT_MAIN : Theme::ACCENT_CALM, 9.5f, true);
        } else {
            // vector pencil & paper icon
            sc(cr, Theme::TEXT_MUTED, alpha * 0.9f);
            cairo_set_line_width(cr, 1.2);
            // paper sheet outline
            cairo_new_path(cr);
            cairo_move_to(cr, editIconX - 6.0, editIconY - 6.0);
            cairo_line_to(cr, editIconX + 1.0, editIconY - 6.0);
            cairo_line_to(cr, editIconX + 4.0, editIconY - 3.0);
            cairo_line_to(cr, editIconX + 4.0, editIconY + 6.0);
            cairo_line_to(cr, editIconX - 6.0, editIconY + 6.0);
            cairo_close_path(cr);
            cairo_stroke(cr);

            // pencil angled across
            sc(cr, Theme::ACCENT_CALM, alpha);
            cairo_set_line_width(cr, 1.5);
            cairo_new_path(cr);
            cairo_move_to(cr, editIconX - 1.0, editIconY + 4.0);
            cairo_line_to(cr, editIconX + 6.5, editIconY - 4.5);
            cairo_stroke(cr);

            drawLabel(cr, editIconX + 12.0, curItemY + 7.0, "Edit",
                      editHover ? Theme::ACCENT_CALM : Theme::TEXT_MAIN, 9.5f, true);
        }

        cairo_restore(cr);
    }
}

void SettingsPanel::drawCompatibilitySection(cairo_t* cr, double x, double y, double w) {
    double cY = y;

    drawLabel(cr, x, cY, "Compatibility", Theme::TEXT_MAIN, 13.f, true);
    cY += 26;

    sc(cr, Theme::BORDER_SOFT, 0.3f);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_move_to(cr, x, cY); cairo_line_to(cr, x + w, cY); cairo_stroke(cr);
    cY += 16;

    drawLabel(cr, x, cY, "User-Agent Identity Preset", Theme::TEXT_MAIN, 10.5f, true);
    cY += 18;
    drawLabel(cr, x, cY, "Select the browser identity sent to websites. Changes apply across all tabs.", Theme::TEXT_DIM, 9.f);
    cY += 22;

    // dropdown trigger button
    double trigH = 46.0;
    m_uaTriggerX = x;
    m_uaTriggerY = cY;
    m_uaTriggerW = w;
    m_uaTriggerH = trigH;

    bool trigHover = (m_hoveredItem == 600);
    rr(cr, x, cY, w, trigH, 7.0);
    sc(cr, trigHover ? Theme::BG_ACTIVE : Theme::BG_SUBTLE);
    cairo_fill_preserve(cr);
    sc(cr, trigHover ? Theme::ACCENT_CALM : Theme::BORDER_SOFT, trigHover ? 0.75f : 0.45f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // active preset details
    std::string activeName = "Chrome 131 (Linux)";
    std::string activeUa = "";
    std::string activePlatform = "Linux";
    if (m_settings.activeUserAgentIndex >= 0 &&
        m_settings.activeUserAgentIndex < static_cast<int>(m_settings.userAgents.size())) {
        const auto& item = m_settings.userAgents[m_settings.activeUserAgentIndex];
        activeName = item.name;
        activeUa = item.userAgent;
        activePlatform = item.platform;
    }

    // Top row: Preset name + platform pill
    drawLabel(cr, x + 14, cY + 7, activeName.c_str(), Theme::TEXT_MAIN, 10.5f, true);

    // Platform pill
    double badgeW = 54.0, badgeH = 16.0;
    double badgeX = x + 14 + activeName.length() * 7.5 + 8.0;
    rr(cr, badgeX, cY + 7, badgeW, badgeH, 4.0);
    sc(cr, Theme::BG_SURFACE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::ACCENT_CALM, 0.45f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    drawLabel(cr, badgeX + 7, cY + 8, activePlatform.c_str(), Theme::ACCENT_CALM, 8.0f, true);

    // Bottom row: exact User-Agent string
    drawLabel(cr, x + 14, cY + 27, activeUa.c_str(), Theme::TEXT_DIM, 8.0f);

    // Chevron icon
    double chX = x + w - 20;
    double chY = cY + trigH / 2.0;
    sc(cr, trigHover ? Theme::TEXT_MAIN : Theme::TEXT_MUTED);
    cairo_set_line_width(cr, 1.6);
    cairo_new_path(cr);
    if (m_uaDropdownOpen) {
        cairo_move_to(cr, chX - 5, chY + 2);
        cairo_line_to(cr, chX, chY - 3);
        cairo_line_to(cr, chX + 5, chY + 2);
    } else {
        cairo_move_to(cr, chX - 5, chY - 2);
        cairo_line_to(cr, chX, chY + 3);
        cairo_line_to(cr, chX + 5, chY - 2);
    }
    cairo_stroke(cr);

    // Animated dropdown list
    if (m_uaDropdownAlpha > 0.005f) {
        double itemH = 38.0;
        int nItems = static_cast<int>(m_settings.userAgents.size());
        double totalH = nItems * itemH + 8.0;
        double dropX = x;
        double dropY = cY + trigH + 6.0;
        double dropW = w;

        m_uaDropdownX = dropX;
        m_uaDropdownY = dropY;
        m_uaDropdownW = dropW;
        m_uaDropdownH = totalH;

        float alpha = m_uaDropdownAlpha;
        double shiftY = (1.0f - alpha) * -6.0;

        cairo_save(cr);
        cairo_translate(cr, 0, shiftY);

        // Drop shadow
        rr(cr, dropX + 2, dropY + 3, dropW, totalH, 8.0);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.40 * alpha);
        cairo_fill(cr);

        // Container background
        rr(cr, dropX, dropY, dropW, totalH, 8.0);
        sc(cr, Theme::BG_SURFACE, alpha);
        cairo_fill_preserve(cr);
        sc(cr, Theme::BORDER_SOFT, 0.65f * alpha);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        double curItemY = dropY + 4.0;
        for (int i = 0; i < nItems; ++i) {
            bool isCurrent = (i == m_settings.activeUserAgentIndex);
            bool isHov = (m_hoveredUaIdx == i);
            const auto& item = m_settings.userAgents[i];

            if (isCurrent || isHov) {
                rr(cr, dropX + 4.0, curItemY, dropW - 8.0, itemH, 6.0);
                if (isCurrent) {
                    sc(cr, Theme::BG_ACTIVE, 0.95f * alpha);
                } else {
                    sc(cr, Theme::BG_SUBTLE, 0.75f * alpha);
                }
                cairo_fill(cr);
            }

            if (isCurrent) {
                rr(cr, dropX + 6.0, curItemY + 5.0, 3.0, itemH - 10.0, 1.5);
                sc(cr, Theme::ACCENT_CALM, alpha);
                cairo_fill(cr);
            }

            // Top line: name and platform badge
            drawLabel(cr, dropX + 16.0, curItemY + 4.0, item.name.c_str(),
                      isCurrent ? Theme::TEXT_MAIN : (isHov ? Theme::TEXT_MAIN : Theme::TEXT_MUTED),
                      9.5f, isCurrent);

            drawLabel(cr, dropX + dropW - 75.0, curItemY + 4.0, item.platform.c_str(),
                      isCurrent ? Theme::ACCENT_CALM : Theme::TEXT_DIM, 8.0f, true);

            // Bottom line: exact User-Agent string as requested
            drawLabel(cr, dropX + 16.0, curItemY + 21.0, item.userAgent.c_str(),
                      isCurrent ? Theme::TEXT_MUTED : Theme::TEXT_DIM, 7.8f);

            curItemY += itemH;
        }

        cairo_restore(cr);
    } else {
        // Closed hint card
        double tipY = cY + trigH + 20.0;
        double tipH = 76.0;
        rr(cr, x, tipY, w, tipH, 8.0);
        sc(cr, Theme::BG_SUBTLE, 0.5f);
        cairo_fill_preserve(cr);
        sc(cr, Theme::BORDER_SOFT, 0.35f);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        // Left accent bar
        rr(cr, x, tipY, 4.0, tipH, 2.0);
        sc(cr, Theme::ACCENT_CALM, 0.85f);
        cairo_fill(cr);

        drawLabel(cr, x + 16, tipY + 12, "Video Streaming & YouTube Compatibility", Theme::TEXT_MAIN, 9.5f, true);
        drawLabel(cr, x + 16, tipY + 31, "Select Chrome (Linux) or Chrome (Windows) to instruct services (YouTube,", Theme::TEXT_DIM, 8.5f);
        drawLabel(cr, x + 16, tipY + 49, "VK, Rutube) to deliver high-performance HTML5 DASH / VP9 streams.", Theme::TEXT_DIM, 8.5f);
    }
}

void SettingsPanel::drawDeleteConfirmModal(cairo_t* cr, double winW, double winH) {
    if (m_deleteConfirmModalAlpha < 0.002f) return;

    float alpha = m_deleteConfirmModalAlpha;

    // backdrop dimming
    cairo_set_source_rgba(cr, 0, 0, 0, 0.65 * alpha);
    cairo_rectangle(cr, 0, 0, winW, winH);
    cairo_fill(cr);

    // modal card geometry
    double mw = std::min(winW - 60.0, 460.0);
    double mh = 175.0;
    double mx = (winW - mw) / 2.0;
    double my = (winH - mh) / 2.0;

    float ease = easeOut(alpha);
    double scale = 0.90 + 0.10 * ease;

    cairo_save(cr);
    cairo_translate(cr, mx + mw / 2.0, my + mh / 2.0);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -(mx + mw / 2.0), -(my + mh / 2.0));

    // drop shadow
    rr(cr, mx + 4, my + 6, mw, mh, 12.0);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.55 * alpha);
    cairo_fill(cr);

    // card background
    rr(cr, mx, my, mw, mh, 12.0);
    sc(cr, Theme::BG_SURFACE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, 0.8f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // danger badge circle with centered minus
    double circleR = 10.0;
    double circleCx = mx + 24.0 + circleR;
    double circleCy = my + 30.0;
    sc(cr, Theme::DANGER);
    cairo_arc(cr, circleCx, circleCy, circleR, 0, 2 * M_PI);
    cairo_fill(cr);

    // white minus inside badge - exactly centered on circle center
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(cr, 2.0);
    cairo_new_path(cr);
    cairo_move_to(cr, circleCx - 4.5, circleCy);
    cairo_line_to(cr, circleCx + 4.5, circleCy);
    cairo_stroke(cr);

    // modal title vertically aligned with circle
    double titleX = circleCx + circleR + 10.0;
    PangoLayout* lTitle = pango_cairo_create_layout(cr);
    PangoFontDescription* fdTitle = pango_font_description_from_string("Inter Bold 12");
    pango_layout_set_font_description(lTitle, fdTitle);
    pango_font_description_free(fdTitle);
    pango_layout_set_text(lTitle, "Delete Search Engine", -1);
    int tlw = 0, tlh = 0;
    pango_layout_get_pixel_size(lTitle, &tlw, &tlh);
    sc(cr, Theme::TEXT_MAIN);
    cairo_move_to(cr, titleX, circleCy - tlh / 2.0);
    pango_cairo_show_layout(cr, lTitle);
    g_object_unref(lTitle);

    // engine name to be deleted
    std::string targetName = "this search engine";
    if (m_deleteTargetEngineIdx >= 0 &&
        m_deleteTargetEngineIdx < static_cast<int>(m_settings.searchEngines.size())) {
        targetName = m_settings.searchEngines[m_deleteTargetEngineIdx].name;
    }

    std::string confirmMsg = "Are you sure you want to delete the search engine \"" + targetName + "\"?";
    PangoLayout* lDesc = pango_cairo_create_layout(cr);
    PangoFontDescription* fd = pango_font_description_from_string("Inter 10");
    pango_layout_set_font_description(lDesc, fd);
    pango_font_description_free(fd);
    pango_layout_set_width(lDesc, static_cast<int>((mw - 48.0) * PANGO_SCALE));
    pango_layout_set_wrap(lDesc, PANGO_WRAP_WORD);
    pango_layout_set_text(lDesc, confirmMsg.c_str(), -1);
    sc(cr, Theme::TEXT_MUTED);
    cairo_move_to(cr, mx + 24.0, my + 58.0);
    pango_cairo_show_layout(cr, lDesc);
    g_object_unref(lDesc);

    // buttons at bottom: Delete is RED, Cancel is BLUE
    double btnW = 90.0, btnH = 34.0;
    double btnY = my + mh - 48.0;
    double btnDeleteX = mx + mw - 24.0 - btnW;
    double btnCancelX = btnDeleteX - 12.0 - btnW;

    // cancel button (blue)
    rr(cr, btnCancelX, btnY, btnW, btnH, 6.0);
    sc(cr, Theme::ACCENT_CALM, m_hoveredDeleteCancel ? 1.0f : 0.85f);
    cairo_fill(cr);
    drawCenteredText(cr, btnCancelX, btnY, btnW, btnH, "Cancel", Theme::BG_ABYSS, 10.f, true);

    // delete button (red)
    rr(cr, btnDeleteX, btnY, btnW, btnH, 6.0);
    sc(cr, Theme::DANGER, m_hoveredDeleteConfirm ? 1.0f : 0.85f);
    cairo_fill(cr);
    drawCenteredText(cr, btnDeleteX, btnY, btnW, btnH, "Delete", Theme::TEXT_MAIN, 10.f, true);

    cairo_restore(cr);
}

void SettingsPanel::drawCreateSearchModal(cairo_t* cr, double winW, double winH) {
    if (m_createSearchModalAlpha < 0.002f) return;

    float alpha = m_createSearchModalAlpha;

    // backdrop dimming
    cairo_set_source_rgba(cr, 0, 0, 0, 0.65 * alpha);
    cairo_rectangle(cr, 0, 0, winW, winH);
    cairo_fill(cr);

    // elongated modal card
    double mw = std::min(winW - 60.0, 560.0);
    double mh = 265.0;
    double mx = (winW - mw) / 2.0;
    double my = (winH - mh) / 2.0;

    float ease = easeOut(alpha);
    double scale = 0.90 + 0.10 * ease;

    cairo_save(cr);
    cairo_translate(cr, mx + mw / 2.0, my + mh / 2.0);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -(mx + mw / 2.0), -(my + mh / 2.0));

    // drop shadow
    rr(cr, mx + 4, my + 6, mw, mh, 12.0);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.55 * alpha);
    cairo_fill(cr);

    // card background
    rr(cr, mx, my, mw, mh, 12.0);
    sc(cr, Theme::BG_SURFACE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, 0.8f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // title header with magnifying glass vector icon
    double iconX = mx + 24.0, iconY = my + 22.0;
    sc(cr, Theme::ACCENT_CALM);
    cairo_set_line_width(cr, 1.8);
    cairo_new_path(cr);
    cairo_arc(cr, iconX + 6, iconY + 6, 5.0, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_new_path(cr);
    cairo_move_to(cr, iconX + 10, iconY + 10);
    cairo_line_to(cr, iconX + 15, iconY + 15);
    cairo_stroke(cr);

    drawLabel(cr, iconX + 24.0, my + 20.0, "Create Search Engine", Theme::TEXT_MAIN, 12.5f, true);

    // description text (exact english wording requested by user)
    PangoLayout* lDesc = pango_cairo_create_layout(cr);
    PangoFontDescription* fd = pango_font_description_from_string("Inter 9.5");
    pango_layout_set_font_description(lDesc, fd);
    pango_font_description_free(fd);
    pango_layout_set_width(lDesc, static_cast<int>((mw - 48.0) * PANGO_SCALE));
    pango_layout_set_wrap(lDesc, PANGO_WRAP_WORD);
    pango_layout_set_spacing(lDesc, static_cast<int>(3 * PANGO_SCALE));
    const char* descText = "lumen allows you to add a search engine of your choice.\n"
                           "Enter your desired search engine in the field below, using %s where the search query should go.";
    pango_layout_set_text(lDesc, descText, -1);
    sc(cr, Theme::TEXT_MUTED);
    cairo_move_to(cr, mx + 24.0, my + 54.0);
    pango_cairo_show_layout(cr, lDesc);
    g_object_unref(lDesc);

    // text input box
    double inpX = mx + 24.0;
    double inpY = my + 118.0;
    double inpW = mw - 48.0;
    double inpH = 38.0;

    bool valid = isCreateSearchValid();
    rr(cr, inpX, inpY, inpW, inpH, 6.0);
    sc(cr, Theme::BG_ACTIVE);
    cairo_fill_preserve(cr);
    if (valid) {
        sc(cr, Theme::ACCENT_CALM, 0.85f);
        cairo_set_line_width(cr, 1.4);
    } else {
        sc(cr, Theme::BORDER_SOFT, 0.7f);
        cairo_set_line_width(cr, 1.0);
    }
    cairo_stroke(cr);

    // input text or placeholder
    cairo_save(cr);
    cairo_rectangle(cr, inpX + 8.0, inpY, inpW - 16.0, inpH);
    cairo_clip(cr);

    if (m_createSearchInput.empty()) {
        drawLabel(cr, inpX + 12.0, inpY + 11.0, "https://example.com/search?q=%s", Theme::TEXT_DIM, 10.f);
    } else {
        // draw selection highlight if active
        if (hasCreateSearchSelection()) {
            int s = std::min(m_createSearchSelStart, m_createSearchSelEnd);
            int e = std::max(m_createSearchSelStart, m_createSearchSelEnd);
            s = std::clamp(s, 0, static_cast<int>(m_createSearchInput.length()));
            e = std::clamp(e, 0, static_cast<int>(m_createSearchInput.length()));

            while (s > 0 && (static_cast<unsigned char>(m_createSearchInput[s]) & 0xC0) == 0x80) --s;
            while (e > 0 && (static_cast<unsigned char>(m_createSearchInput[e]) & 0xC0) == 0x80) --e;

            PangoLayout* lSub = pango_cairo_create_layout(cr);
            PangoFontDescription* fdSub = pango_font_description_from_string("Inter 10");
            pango_layout_set_font_description(lSub, fdSub);
            pango_font_description_free(fdSub);

            pango_layout_set_text(lSub, m_createSearchInput.substr(0, s).c_str(), -1);
            int w1 = 0, h1 = 0;
            pango_layout_get_pixel_size(lSub, &w1, &h1);

            pango_layout_set_text(lSub, m_createSearchInput.substr(0, e).c_str(), -1);
            int w2 = 0, h2 = 0;
            pango_layout_get_pixel_size(lSub, &w2, &h2);
            g_object_unref(lSub);

            sc(cr, Theme::ACCENT_CALM, 0.35f);
            cairo_rectangle(cr, inpX + 12.0 + w1, inpY + 6.0, std::max(2.0, static_cast<double>(w2 - w1)), inpH - 12.0);
            cairo_fill(cr);
        }

        drawLabel(cr, inpX + 12.0, inpY + 11.0, m_createSearchInput.c_str(), Theme::TEXT_MAIN, 10.f);

        // draw caret at cursor position with utf-8 boundary safety
        PangoLayout* lCaret = pango_cairo_create_layout(cr);
        PangoFontDescription* fdCaret = pango_font_description_from_string("Inter 10");
        pango_layout_set_font_description(lCaret, fdCaret);
        pango_font_description_free(fdCaret);

        int curPos = std::clamp(m_createSearchCursor, 0, static_cast<int>(m_createSearchInput.length()));
        while (curPos > 0 && (static_cast<unsigned char>(m_createSearchInput[curPos]) & 0xC0) == 0x80) {
            --curPos;
        }
        std::string sub = m_createSearchInput.substr(0, curPos);
        pango_layout_set_text(lCaret, sub.c_str(), -1);
        int cw = 0, ch = 0;
        pango_layout_get_pixel_size(lCaret, &cw, &ch);
        g_object_unref(lCaret);

        double caretX = inpX + 12.0 + cw;
        sc(cr, Theme::ACCENT_CALM);
        cairo_set_line_width(cr, 1.4);
        cairo_new_path(cr);
        cairo_move_to(cr, caretX, inpY + 8.0);
        cairo_line_to(cr, caretX, inpY + inpH - 8.0);
        cairo_stroke(cr);
    }
    cairo_restore(cr);

    // buttons at bottom
    double btnW = 90.0, btnH = 34.0;
    double btnY = my + mh - 50.0;
    double btnCreateX = mx + mw - 24.0 - btnW;
    double btnCancelX = btnCreateX - 12.0 - btnW;

    // cancel button (gray)
    rr(cr, btnCancelX, btnY, btnW, btnH, 6.0);
    sc(cr, m_hoveredModalCancel ? Theme::BG_ACTIVE : Theme::BG_SUBTLE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, m_hoveredModalCancel ? 0.9f : 0.6f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    drawCenteredText(cr, btnCancelX, btnY, btnW, btnH, "Cancel",
                     m_hoveredModalCancel ? Theme::TEXT_MAIN : Theme::TEXT_MUTED, 10.f, false);

    // create button (blue/accent, disabled until valid)
    rr(cr, btnCreateX, btnY, btnW, btnH, 6.0);
    if (valid) {
        sc(cr, Theme::ACCENT_CALM, m_hoveredModalCreate ? 1.0f : 0.85f);
        cairo_fill(cr);
        drawCenteredText(cr, btnCreateX, btnY, btnW, btnH, "Create", Theme::BG_ABYSS, 10.f, true);
    } else {
        sc(cr, Theme::ACCENT_CALM, 0.22f);
        cairo_fill(cr);
        drawCenteredText(cr, btnCreateX, btnY, btnW, btnH, "Create", Theme::TEXT_DIM, 10.f, false);
    }

    cairo_restore(cr);
}

void SettingsPanel::drawLumenThresholdModal(cairo_t* cr, double winW, double winH) {
    if (m_lumenThresholdModalAlpha < 0.002f) return;

    float alpha = m_lumenThresholdModalAlpha;

    // backdrop dimming
    cairo_set_source_rgba(cr, 0, 0, 0, 0.65 * alpha);
    cairo_rectangle(cr, 0, 0, winW, winH);
    cairo_fill(cr);

    // modal card
    double mw = std::min(winW - 60.0, 480.0);
    double mh = 210.0;
    double mx = (winW - mw) / 2.0;
    double my = (winH - mh) / 2.0;

    // smooth scale entry
    cairo_save(cr);
    double scale = 0.94 + 0.06 * alpha;
    cairo_translate(cr, mx + mw / 2.0, my + mh / 2.0);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -(mx + mw / 2.0), -(my + mh / 2.0));

    // drop shadow
    rr(cr, mx + 4, my + 6, mw, mh, 12.0);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.55 * alpha);
    cairo_fill(cr);

    // card background
    rr(cr, mx, my, mw, mh, 12.0);
    sc(cr, Theme::BG_SURFACE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, 0.85f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // crescent moon night glow vector icon
    double iconCx = mx + 32.0;
    double iconCy = my + 30.0;
    double moonR = 9.5;
    sc(cr, Theme::ACCENT_CALM);
    cairo_new_path(cr);
    cairo_arc(cr, iconCx, iconCy, moonR, -0.6 * M_PI, 0.6 * M_PI);
    cairo_arc_negative(cr, iconCx + 5.0, iconCy, moonR * 0.85, 0.5 * M_PI, -0.5 * M_PI);
    cairo_close_path(cr);
    cairo_fill(cr);

    // modal title header
    double titleX = mx + 52.0;
    PangoLayout* lTitle = pango_cairo_create_layout(cr);
    PangoFontDescription* fdTitle = pango_font_description_from_string("Inter Bold 12.5");
    pango_layout_set_font_description(lTitle, fdTitle);
    pango_font_description_free(fdTitle);
    pango_layout_set_text(lTitle, "lumen threshold", -1);
    int tlw = 0, tlh = 0;
    pango_layout_get_pixel_size(lTitle, &tlw, &tlh);
    sc(cr, Theme::TEXT_MAIN);
    cairo_move_to(cr, titleX, iconCy - tlh / 2.0);
    pango_cairo_show_layout(cr, lTitle);
    g_object_unref(lTitle);

    // prompt content in English
    PangoLayout* lDesc = pango_cairo_create_layout(cr);
    PangoFontDescription* fd = pango_font_description_from_string("Inter 10");
    pango_layout_set_font_description(lDesc, fd);
    pango_font_description_free(fd);
    pango_layout_set_width(lDesc, static_cast<int>((mw - 48.0) * PANGO_SCALE));
    pango_layout_set_wrap(lDesc, PANGO_WRAP_WORD);
    pango_layout_set_spacing(lDesc, static_cast<int>(3 * PANGO_SCALE));
    const char* descText = "It looks like it's already dark outside. Are you sure you want to switch to a light theme?\n\n"
                           "High luminence at night may cause visual fatigue and eye strain.";
    pango_layout_set_text(lDesc, descText, -1);
    sc(cr, Theme::TEXT_MUTED);
    cairo_move_to(cr, mx + 24.0, my + 60.0);
    pango_cairo_show_layout(cr, lDesc);
    g_object_unref(lDesc);

    // bottom buttons: Gray "Cancel", Blue "Accept"
    double btnW = 92.0, btnH = 34.0;
    double btnY = my + mh - 48.0;
    double btnAcceptX = mx + mw - 24.0 - btnW;
    double btnCancelX = btnAcceptX - 12.0 - btnW;

    // cancel button (gray)
    rr(cr, btnCancelX, btnY, btnW, btnH, 6.0);
    sc(cr, m_hoveredLumenCancel ? Theme::BG_ACTIVE : Theme::BG_SUBTLE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, m_hoveredLumenCancel ? 0.9f : 0.6f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    drawCenteredText(cr, btnCancelX, btnY, btnW, btnH, "Cancel",
                     m_hoveredLumenCancel ? Theme::TEXT_MAIN : Theme::TEXT_MUTED, 10.f, false);

    // accept button (blue)
    rr(cr, btnAcceptX, btnY, btnW, btnH, 6.0);
    Theme::Color blueCol = Theme::Color::fromHex(0x2D638E);
    sc(cr, blueCol, m_hoveredLumenAccept ? 1.0f : 0.88f);
    cairo_fill(cr);
    drawCenteredText(cr, btnAcceptX, btnY, btnW, btnH, "Accept",
                     Theme::Color{1.0f, 1.0f, 1.0f, 1.0f}, 10.f, true);

    cairo_restore(cr);
}

void SettingsPanel::drawSidebar(cairo_t* cr, double x, double y, double w, double h) {
    static const char* tabs[] = { "Animations", "Appearance", "Search", "Compatibility", "AI Core" };
    int nTabs = 5;
    double tabH = 36.0, gap = 4.0;
    double tabStartY = y + 8.0;

    // smooth gliding active cursor pill
    double activeY = tabStartY + m_sidebarCursorY * (tabH + gap);
    rr(cr, x + 8.0, activeY, w - 16.0, tabH, 6.0);
    sc(cr, Theme::BG_ACTIVE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::ACCENT_CALM, 0.45f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // left accent pill on active cursor
    rr(cr, x + 8.0, activeY + 6.0, 3.0, tabH - 12.0, 1.5);
    sc(cr, Theme::ACCENT_CALM);
    cairo_fill(cr);

    // tab items and labels
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
    // clipping
    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_clip(cr);

    double cX = x + 16, cY = y + 16, cW = w - 32;

    if (section == 0)      drawAnimationsSection(cr, cX, cY, cW);
    else if (section == 1) drawAppearanceSection(cr, cX, cY, cW, h - 32);
    else if (section == 2) drawSearchSection(cr, cX, cY, cW);
    else if (section == 3) drawCompatibilitySection(cr, cX, cY, cW);
    else if (section == 4) drawComingSoon(cr, cX, cY, "AI Core");

    cairo_restore(cr);
}

void SettingsPanel::drawPanel(cairo_t* cr) {
    // panel dimensions
    double pw = m_pw, ph = m_ph, px = m_px, py = m_py;

    // shadow
    rr(cr, px + 5, py + 5, pw, ph, 12);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
    cairo_fill(cr);

    // panel bg
    rr(cr, px, py, pw, ph, 12);
    sc(cr, Theme::BG_SURFACE);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, 0.6f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // title bar
    cairo_save(cr);
    rr(cr, px, py, pw, 44, 12);
    cairo_clip(cr);
    rr(cr, px, py, pw, 44, 0);
    sc(cr, Theme::BG_SUBTLE);
    cairo_fill(cr);
    cairo_restore(cr);

    drawLabel(cr, px + 16, py + 15, "Settings", Theme::TEXT_MAIN, 12.f, true);

    // close x button
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

    // divider
    sc(cr, Theme::BORDER_SOFT, 0.35f);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_move_to(cr, px,    py + 44);
    cairo_line_to(cr, px+pw, py + 44);
    cairo_stroke(cr);

    // body
    double bodyY = py + 44;
    double bodyH = ph - 44;
    double sideW = 130.0;

    drawSidebar(cr, px, bodyY, sideW, bodyH);

    // vertical divider
    sc(cr, Theme::BORDER_SOFT, 0.3f);
    cairo_set_line_width(cr, 1.0);
    cairo_new_path(cr);
    cairo_move_to(cr, px + sideW, bodyY + 10);
    cairo_line_to(cr, px + sideW, bodyY + bodyH - 10);
    cairo_stroke(cr);

    // animated section content
    double contentX = px + sideW + 1;
    double contentW = pw - sideW - 1;

    cairo_save(cr);
    cairo_rectangle(cr, contentX, bodyY, contentW, bodyH);
    cairo_clip(cr);

    float sp = m_sectionProg;
    if (m_sectAnim.isRunning()) {
        constexpr double maxShift = 30.0;
        double outOff = -m_sectionDir * sp * maxShift;
        cairo_push_group(cr);
        drawContent(cr, contentX, bodyY + outOff, contentW, bodyH, m_prevSection);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, std::clamp(1.0f - sp, 0.0f, 1.0f));

        double inOff = m_sectionDir * (1.0f - sp) * maxShift;
        cairo_push_group(cr);
        drawContent(cr, contentX, bodyY + inOff, contentW, bodyH, m_section);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, std::clamp(sp, 0.0f, 1.0f));
    } else {
        drawContent(cr, contentX, bodyY, contentW, bodyH, m_section);
    }
    cairo_restore(cr);

}

void SettingsPanel::draw(cairo_t* cr, double winW, double winH) {
    m_winW = winW;
    m_winH = winH;

    float p = m_openProg;
    if (p < 0.001f && m_createSearchModalAlpha < 0.001f && m_deleteConfirmModalAlpha < 0.001f) return;

    if (p >= 0.001f) {
        // overlay dim
        cairo_set_source_rgba(cr, 0, 0, 0, 0.5 * p);
        cairo_rectangle(cr, 0, 0, winW, winH);
        cairo_fill(cr);

        // compute panel geometry
        m_pw = std::min(winW - 80, 720.0);
        m_ph = std::min(winH - 80, 500.0);
        m_px = (winW - m_pw) / 2.0;
        m_py = (winH - m_ph) / 2.0;

        float ease = easeOut(p);
        double scale = 0.88 + 0.12 * ease;
        double alpha = ease;

        cairo_push_group(cr);
        double pcx = m_px + m_pw / 2.0;
        double pcy = m_py + m_ph / 2.0;
        cairo_translate(cr, pcx, pcy);
        cairo_scale(cr, scale, scale);
        cairo_translate(cr, -pcx, -pcy);

        drawPanel(cr);

        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, alpha);
    }

    drawLumenThresholdModal(cr, winW, winH);
    drawDeleteConfirmModal(cr, winW, winH);
    drawCreateSearchModal(cr, winW, winH);
}

bool SettingsPanel::isInBounds(double mx, double my) const {
    return mx >= m_px && mx <= m_px + m_pw && my >= m_py && my <= m_py + m_ph;
}

bool SettingsPanel::handleMouseMove(double mx, double my) {
    if (!isVisible()) return false;

    // if lumen threshold modal is open
    if (m_lumenThresholdModalOpen) {
        double mw = std::min(m_winW - 60.0, 480.0);
        double mh = 210.0;
        double modalX = (m_winW - mw) / 2.0;
        double modalY = (m_winH - mh) / 2.0;

        double btnW = 92.0, btnH = 34.0;
        double btnY = modalY + mh - 48.0;
        double btnAcceptX = modalX + mw - 24.0 - btnW;
        double btnCancelX = btnAcceptX - 12.0 - btnW;

        m_hoveredLumenCancel = (mx >= btnCancelX && mx <= btnCancelX + btnW && my >= btnY && my <= btnY + btnH);
        m_hoveredLumenAccept = (mx >= btnAcceptX && mx <= btnAcceptX + btnW && my >= btnY && my <= btnY + btnH);
        return true;
    }

    // if delete modal is open
    if (m_deleteConfirmModalOpen) {
        double mw = std::min(m_winW - 60.0, 460.0);
        double mh = 175.0;
        double modalX = (m_winW - mw) / 2.0;
        double modalY = (m_winH - mh) / 2.0;

        double btnW = 90.0, btnH = 34.0;
        double btnY = modalY + mh - 48.0;
        double btnDeleteX = modalX + mw - 24.0 - btnW;
        double btnCancelX = btnDeleteX - 12.0 - btnW;

        m_hoveredDeleteCancel = (mx >= btnCancelX && mx <= btnCancelX + btnW && my >= btnY && my <= btnY + btnH);
        m_hoveredDeleteConfirm = (mx >= btnDeleteX && mx <= btnDeleteX + btnW && my >= btnY && my <= btnY + btnH);
        return true;
    }

    // if create search engine modal is open
    if (m_createSearchModalOpen) {
        double mw = std::min(m_winW - 60.0, 560.0);
        double mh = 265.0;
        double modalX = (m_winW - mw) / 2.0;
        double modalY = (m_winH - mh) / 2.0;

        double btnW = 90.0, btnH = 34.0;
        double btnY = modalY + mh - 50.0;
        double btnCreateX = modalX + mw - 24.0 - btnW;
        double btnCancelX = btnCreateX - 12.0 - btnW;

        m_hoveredModalCancel = (mx >= btnCancelX && mx <= btnCancelX + btnW && my >= btnY && my <= btnY + btnH);
        m_hoveredModalCreate = (mx >= btnCreateX && mx <= btnCreateX + btnW && my >= btnY && my <= btnY + btnH);
        return true;
    }

    // continuous slider dragging across all 5 sliders
    if (m_dragSlider >= 0) {
        float* vals[] = {
            &m_settings.anim.tabSlide,
            &m_settings.anim.tabClose,
            &m_settings.anim.settingsOpen,
            &m_settings.anim.sectionSwitch,
            &m_settings.anim.reloadSpin,
        };
        if (m_dragSlider >= 0 && m_dragSlider < 5 && m_dragSliderW > 0.0) {
            double t = (mx - m_dragSliderX0) / m_dragSliderW;
            t = std::clamp(t, 0.0, 1.0);
            *vals[m_dragSlider] = static_cast<float>(0.25 + t * (3.0 - 0.25));
            m_sliderIsDragging = true;
            return true;
        }
    }

    // search section dropdown hover
    if (m_section == 2 && m_searchDropdownOpen && m_searchDropdownAlpha > 0.1f) {
        if (mx >= m_dropdownX && mx <= m_dropdownX + m_dropdownW &&
            my >= m_dropdownY && my <= m_dropdownY + m_dropdownH) {
            double relY = my - m_dropdownY - 4.0;
            double itemH = 32.0;
            int nItems = static_cast<int>(m_settings.searchEngines.size());
            int idx = static_cast<int>(relY / itemH);
            if (idx >= 0 && idx < nItems) {
                // if hovering red minus on custom engine in edit mode
                if (m_searchEditMode && m_settings.searchEngines[idx].isCustom &&
                    mx >= m_dropdownX + 4.0 && mx <= m_dropdownX + 28.0) {
                    m_hoveredDropdownIdx = 1000 + idx;
                } else {
                    m_hoveredDropdownIdx = idx;
                }
            } else {
                double afterY = relY - nItems * itemH;
                if (m_searchEditMode) {
                    if (afterY >= 0.0 && afterY < 32.0) {
                        m_hoveredDropdownIdx = 997; // + Create
                    } else if (afterY >= 32.0) {
                        m_hoveredDropdownIdx = 998; // Done
                    } else {
                        m_hoveredDropdownIdx = -1;
                    }
                } else {
                    if (afterY >= 0.0) {
                        m_hoveredDropdownIdx = 998; // Edit
                    } else {
                        m_hoveredDropdownIdx = -1;
                    }
                }
            }
            return true;
        } else {
            m_hoveredDropdownIdx = -1;
        }
    }

    // compatibility section user agent dropdown hover
    if (m_section == 3 && m_uaDropdownOpen && m_uaDropdownAlpha > 0.1f) {
        if (mx >= m_uaDropdownX && mx <= m_uaDropdownX + m_uaDropdownW &&
            my >= m_uaDropdownY && my <= m_uaDropdownY + m_uaDropdownH) {
            double relY = my - m_uaDropdownY - 4.0;
            double itemH = 38.0;
            int nItems = static_cast<int>(m_settings.userAgents.size());
            int idx = static_cast<int>(relY / itemH);
            if (idx >= 0 && idx < nItems) {
                m_hoveredUaIdx = idx;
            } else {
                m_hoveredUaIdx = -1;
            }
            return true;
        } else {
            m_hoveredUaIdx = -1;
        }
    }

    int old = m_hoveredItem;
    m_hoveredItem = -1;
    if (!isInBounds(mx, my)) return false;

    // close button
    double cX = m_px + m_pw - 22, cY = m_py + 22;
    if (std::hypot(mx - cX, my - cY) <= 12) { m_hoveredItem = 9999; }

    // sidebar tabs (5 tabs)
    double bodyY = m_py + 44;
    double tabH = 36.0, gap = 4.0;
    double tabStartY = bodyY + 8;
    for (int i = 0; i < 5; ++i) {
        double ty = tabStartY + i * (tabH + gap);
        if (mx >= m_px + 5 && mx <= m_px + 125 && my >= ty && my <= ty + tabH) {
            m_hoveredItem = 1000 + i;
            break;
        }
    }

    // toggle and slider hitboxes for animations section
    if (m_section == 0) {
        double sideW = 130.0;
        double cX2 = m_px + sideW + 17;
        double cW  = m_pw - sideW - 33;
        double sY  = bodyY + 64;

        // toggle hitbox
        if (mx >= cX2 - 4 && mx <= cX2 + 220 && my >= sY - 4 && my <= sY + 28) {
            m_hoveredItem = 200;
        }

        // sliders with generous hitbox (5 sliders)
        if (m_settings.anim.enabled) {
            double sliderY = sY + 36;
            for (int si = 0; si < 5; ++si) {
                if (mx >= cX2 - 10 && mx <= cX2 + cW + 10 && my >= sliderY && my <= sliderY + 38) {
                    m_hoveredItem = 10 + si;
                }
                sliderY += 46;
            }
        }
    }

    // appearance section theme cards hover
    if (m_section == 1) {
        m_hoveredThemeIdx = -1;
        if (my >= m_appearanceY && my <= m_appearanceY + m_appearanceH) {
            const auto& palettes = Theme::ThemeManager::instance().allPalettes();
            for (size_t i = 0; i < palettes.size(); ++i) {
                double rx = 0, ry = 0, rw = 0, rh = 0;
                if (getThemeCardRect(i, m_appearanceX, m_appearanceY, m_appearanceW, m_appearanceScrollY, rx, ry, rw, rh)) {
                    if (mx >= rx && mx <= rx + rw && my >= ry && my <= ry + rh) {
                        m_hoveredThemeIdx = static_cast<int>(i);
                        m_hoveredItem = 500 + static_cast<int>(i);
                        break;
                    }
                }
            }
        }
    } else {
        m_hoveredThemeIdx = -1;
    }

    // search section dropdown trigger
    if (m_section == 2) {
        if (mx >= m_searchTriggerX && mx <= m_searchTriggerX + m_searchTriggerW &&
            my >= m_searchTriggerY && my <= m_searchTriggerY + m_searchTriggerH) {
            m_hoveredItem = 300;
        }
    }

    // compatibility section dropdown trigger
    if (m_section == 3) {
        if (mx >= m_uaTriggerX && mx <= m_uaTriggerX + m_uaTriggerW &&
            my >= m_uaTriggerY && my <= m_uaTriggerY + m_uaTriggerH) {
            m_hoveredItem = 600;
        }
    }

    return old != m_hoveredItem;
}

bool SettingsPanel::handleMouseDown(double mx, double my) {
    if (!isVisible()) return false;

    // if lumen threshold modal is open, absorb all clicks
    if (m_lumenThresholdModalOpen) {
        double mw = std::min(m_winW - 60.0, 480.0);
        double mh = 210.0;
        double modalX = (m_winW - mw) / 2.0;
        double modalY = (m_winH - mh) / 2.0;

        double btnW = 92.0, btnH = 34.0;
        double btnY = modalY + mh - 48.0;
        double btnAcceptX = modalX + mw - 24.0 - btnW;
        double btnCancelX = btnAcceptX - 12.0 - btnW;

        if (mx >= btnCancelX && mx <= btnCancelX + btnW && my >= btnY && my <= btnY + btnH) {
            m_lumenThresholdModalOpen = false;
            return true;
        }
        if (mx >= btnAcceptX && mx <= btnAcceptX + btnW && my >= btnY && my <= btnY + btnH) {
            m_lumenThresholdModalOpen = false;
            Theme::ThemeManager::instance().setTheme(m_pendingLightTheme, true);
            return true;
        }
        return true;
    }

    // if delete confirmation modal is open, absorb all clicks
    if (m_deleteConfirmModalOpen) {
        double mw = std::min(m_winW - 60.0, 460.0);
        double mh = 175.0;
        double modalX = (m_winW - mw) / 2.0;
        double modalY = (m_winH - mh) / 2.0;

        double btnW = 90.0, btnH = 34.0;
        double btnY = modalY + mh - 48.0;
        double btnDeleteX = modalX + mw - 24.0 - btnW;
        double btnCancelX = btnDeleteX - 12.0 - btnW;

        if (mx >= btnCancelX && mx <= btnCancelX + btnW && my >= btnY && my <= btnY + btnH) {
            m_deleteConfirmModalOpen = false;
            m_deleteTargetEngineIdx = -1;
            return true;
        }
        if (mx >= btnDeleteX && mx <= btnDeleteX + btnW && my >= btnY && my <= btnY + btnH) {
            deleteCustomSearchEngine(m_deleteTargetEngineIdx);
            return true;
        }
        return true;
    }

    // if create search modal is open, absorb all clicks
    if (m_createSearchModalOpen) {
        double mw = std::min(m_winW - 60.0, 560.0);
        double mh = 265.0;
        double modalX = (m_winW - mw) / 2.0;
        double modalY = (m_winH - mh) / 2.0;

        double btnW = 90.0, btnH = 34.0;
        double btnY = modalY + mh - 50.0;
        double btnCreateX = modalX + mw - 24.0 - btnW;
        double btnCancelX = btnCreateX - 12.0 - btnW;

        // cancel button
        if (mx >= btnCancelX && mx <= btnCancelX + btnW && my >= btnY && my <= btnY + btnH) {
            m_createSearchModalOpen = false;
            return true;
        }
        // create button
        if (mx >= btnCreateX && mx <= btnCreateX + btnW && my >= btnY && my <= btnY + btnH) {
            if (isCreateSearchValid()) {
                commitCustomSearchEngine();
            }
            return true;
        }
        // input field click
        double inpX = modalX + 24.0, inpY = modalY + 118.0, inpW = mw - 48.0, inpH = 38.0;
        if (mx >= inpX && mx <= inpX + inpW && my >= inpY && my <= inpY + inpH) {
            m_createSearchCursor = static_cast<int>(m_createSearchInput.length());
            m_createSearchSelStart = -1;
            m_createSearchSelEnd = -1;
            return true;
        }

        return true;
    }

    // dropdown clicks
    if (m_section == 2 && m_searchDropdownOpen) {
        if (mx >= m_dropdownX && mx <= m_dropdownX + m_dropdownW &&
            my >= m_dropdownY && my <= m_dropdownY + m_dropdownH) {
            double relY = my - m_dropdownY - 4.0;
            double itemH = 32.0;
            int nItems = static_cast<int>(m_settings.searchEngines.size());
            int idx = static_cast<int>(relY / itemH);
            if (idx >= 0 && idx < nItems) {
                // red minus icon clicked
                if (m_searchEditMode && m_settings.searchEngines[idx].isCustom &&
                    mx >= m_dropdownX + 4.0 && mx <= m_dropdownX + 28.0) {
                    triggerDeleteCustomSearchEngine(idx);
                    return true;
                }
                if (!m_searchEditMode) {
                    m_settings.activeSearchEngineIndex = idx;
                    saveActiveEngine();
                    m_searchDropdownOpen = false;
                    return true;
                }
                return true;
            } else {
                double afterY = relY - nItems * itemH;
                if (m_searchEditMode) {
                    if (afterY >= 0.0 && afterY < 32.0) {
                        // + Create
                        m_searchDropdownOpen = false;
                        m_createSearchModalOpen = true;
                        m_createSearchInput = "https://";
                        m_createSearchCursor = static_cast<int>(m_createSearchInput.length());
                        m_createSearchSelStart = -1;
                        m_createSearchSelEnd = -1;
                        return true;
                    } else if (afterY >= 32.0) {
                        // Done
                        m_searchEditMode = false;
                        return true;
                    }
                } else {
                    if (afterY >= 0.0) {
                        // Edit
                        m_searchEditMode = true;
                        return true;
                    }
                }
            }
        }
    }

    // user agent dropdown click in section 3
    if (m_section == 3 && m_uaDropdownOpen) {
        if (mx >= m_uaDropdownX && mx <= m_uaDropdownX + m_uaDropdownW &&
            my >= m_uaDropdownY && my <= m_uaDropdownY + m_uaDropdownH) {
            double relY = my - m_uaDropdownY - 4.0;
            double itemH = 38.0;
            int nItems = static_cast<int>(m_settings.userAgents.size());
            int idx = static_cast<int>(relY / itemH);
            if (idx >= 0 && idx < nItems) {
                m_settings.activeUserAgentIndex = idx;
                saveActiveUserAgent();
                if (m_onUserAgentChanged) {
                    m_onUserAgentChanged(m_settings.getActiveUserAgent());
                }
                m_uaDropdownOpen = false;
                return true;
            }
            return true;
        }
    }

    // click outside closes panel
    if (!isInBounds(mx, my)) {
        if (m_searchDropdownOpen) {
            m_searchDropdownOpen = false;
            m_searchEditMode = false;
            return true;
        }
        if (m_uaDropdownOpen) {
            m_uaDropdownOpen = false;
            return true;
        }
        setVisible(false);
        return true;
    }

    // close button (x)
    double cX = m_px + m_pw - 22, cY = m_py + 22;
    if (std::hypot(mx - cX, my - cY) <= 12) {
        m_searchDropdownOpen = false;
        m_searchEditMode = false;
        m_uaDropdownOpen = false;
        setVisible(false);
        return true;
    }

    // sidebar navigation tabs (5 tabs)
    double bodyY = m_py + 44;
    double tabH = 36.0, gap = 4.0;
    double tabStartY = bodyY + 8;
    for (int i = 0; i < 5; ++i) {
        double ty = tabStartY + i * (tabH + gap);
        if (mx >= m_px + 5 && mx <= m_px + 125 && my >= ty && my <= ty + tabH) {
            m_searchDropdownOpen = false;
            m_searchEditMode = false;
            m_uaDropdownOpen = false;
            sectionSwitch(i);
            return true;
        }
    }

    // toggle: animations enabled
    if (m_section == 0) {
        double sideW = 130.0;
        double cX2   = m_px + sideW + 17;
        double sY    = bodyY + 64;

        if (mx >= cX2 - 4 && mx <= cX2 + 220 && my >= sY - 4 && my <= sY + 28) {
            m_settings.anim.enabled = !m_settings.anim.enabled;
            return true;
        }

        // animation speed sliders (5 sliders)
        if (m_settings.anim.enabled) {
            double cW  = m_pw - sideW - 33;
            double sliderY = sY + 36;
            float* vals[] = {
                &m_settings.anim.tabSlide,
                &m_settings.anim.tabClose,
                &m_settings.anim.settingsOpen,
                &m_settings.anim.sectionSwitch,
                &m_settings.anim.reloadSpin,
            };
            for (int si = 0; si < 5; ++si) {
                if (mx >= cX2 - 10 && mx <= cX2 + cW + 10 && my >= sliderY && my <= sliderY + 38) {
                    double t = (mx - cX2) / cW;
                    t = std::clamp(t, 0.0, 1.0);
                    *vals[si] = static_cast<float>(0.25 + t * (3.0 - 0.25));
                    m_dragSlider       = si;
                    m_dragSliderX0     = cX2;
                    m_dragSliderW      = cW;
                    m_sliderIsDragging = false; // smooth glide to clicked spot
                    return true;
                }
                sliderY += 46;
            }
        }
    }

    // appearance section: theme selection
    if (m_section == 1) {
        if (my >= m_appearanceY && my <= m_appearanceY + m_appearanceH) {
            const auto& palettes = Theme::ThemeManager::instance().allPalettes();
            for (size_t i = 0; i < palettes.size(); ++i) {
                double rx = 0, ry = 0, rw = 0, rh = 0;
                if (getThemeCardRect(i, m_appearanceX, m_appearanceY, m_appearanceW, m_appearanceScrollY, rx, ry, rw, rh)) {
                    if (mx >= rx && mx <= rx + rw && my >= ry && my <= ry + rh) {
                        Theme::ThemeId targetTheme = palettes[i].id;
                        if (Theme::ThemeManager::isLightTheme(targetTheme) && Theme::ThemeManager::isNightTime()) {
                            m_pendingLightTheme = targetTheme;
                            m_lumenThresholdModalOpen = true;
                        } else {
                            Theme::ThemeManager::instance().setTheme(targetTheme, true);
                        }
                        return true;
                    }
                }
            }
        }
    }

    // search section: dropdown trigger
    if (m_section == 2) {
        if (mx >= m_searchTriggerX && mx <= m_searchTriggerX + m_searchTriggerW &&
            my >= m_searchTriggerY && my <= m_searchTriggerY + m_searchTriggerH) {
            m_searchDropdownOpen = !m_searchDropdownOpen;
            if (!m_searchDropdownOpen) m_searchEditMode = false;
            return true;
        } else if (m_searchDropdownOpen) {
            m_searchDropdownOpen = false;
            m_searchEditMode = false;
            return true;
        }
    }

    // compatibility section: dropdown trigger
    if (m_section == 3) {
        if (mx >= m_uaTriggerX && mx <= m_uaTriggerX + m_uaTriggerW &&
            my >= m_uaTriggerY && my <= m_uaTriggerY + m_uaTriggerH) {
            m_uaDropdownOpen = !m_uaDropdownOpen;
            return true;
        } else if (m_uaDropdownOpen) {
            m_uaDropdownOpen = false;
            return true;
        }
    }

    return true; // absorb clicks while panel is open
}

bool SettingsPanel::handleMouseUp(double /*mx*/, double /*my*/) {
    if (m_dragSlider >= 0) {
        m_dragSlider = -1;
        m_sliderIsDragging = false;
        return true;
    }
    return false;
}

bool SettingsPanel::handleScroll(double dy) {
    if (!isVisible()) return false;
    if (m_section == 1) { // Appearance section
        if (m_appearanceMaxScroll > 0.0f) {
            double step = 38.0;
            m_appearanceTargetScrollY += static_cast<float>(dy * step);
            m_appearanceTargetScrollY = std::clamp(m_appearanceTargetScrollY, 0.0f, m_appearanceMaxScroll);
            return true;
        }
    }
    return false;
}

bool SettingsPanel::handleKeyPress(uint32_t sym, uint32_t mod, const char* text) {
    if (!isVisible()) return false;

    // if lumen threshold modal is open
    if (m_lumenThresholdModalOpen) {
        if (sym == SDLK_ESCAPE || sym == 0xff1b || sym == 27) {
            m_lumenThresholdModalOpen = false;
            return true;
        }
        if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == 0xff0d || sym == 13) {
            m_lumenThresholdModalOpen = false;
            Theme::ThemeManager::instance().setTheme(m_pendingLightTheme, true);
            return true;
        }
        return true;
    }

    // if delete confirmation modal is open
    if (m_deleteConfirmModalOpen) {
        if (sym == SDLK_ESCAPE || sym == 0xff1b || sym == 27) {
            m_deleteConfirmModalOpen = false;
            m_deleteTargetEngineIdx = -1;
            return true;
        }
        if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == 0xff0d || sym == 13) {
            deleteCustomSearchEngine(m_deleteTargetEngineIdx);
            return true;
        }
        return true;
    }

    // if create search modal is open, forward all typing to modal input
    if (m_createSearchModalOpen) {
        bool isCtrl = (mod & 4) || (mod & 0x40) || (SDL_GetModState() & KMOD_CTRL);

        // escape: close modal
        if (sym == SDLK_ESCAPE || sym == 0xff1b || sym == 27) {
            m_createSearchModalOpen = false;
            clearCreateSearchSelection();
            return true;
        }
        // enter: commit if valid
        if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == 0xff0d || sym == 13) {
            if (isCreateSearchValid()) {
                commitCustomSearchEngine();
            }
            return true;
        }

        // key definitions for both english and russian / cyrillic layouts
        bool isKeyA = (sym == 'a' || sym == 'A' || sym == 0x0061 || sym == 0x0041 ||
                       sym == 0x06c6 || sym == 0x06e6 || sym == 0x0444 || sym == 0x0424);
        bool isKeyC = (sym == 'c' || sym == 'C' || sym == 0x0063 || sym == 0x0043 ||
                       sym == 0x06d3 || sym == 0x06f3 || sym == 0x0441 || sym == 0x0421);
        bool isKeyV = (sym == 'v' || sym == 'V' || sym == 0x0076 || sym == 0x0056 ||
                       sym == 0x06cd || sym == 0x06ed || sym == 0x043c || sym == 0x041c);
        bool isKeyX = (sym == 'x' || sym == 'X' || sym == 0x0078 || sym == 0x0058 ||
                       sym == 0x06de || sym == 0x06fe || sym == 0x0447 || sym == 0x0427);
        bool isKeyBackspace = (sym == SDLK_BACKSPACE || sym == 0xff08 || sym == 8 || sym == 127);

        // ctrl+a: select all
        if (isKeyA && isCtrl) {
            m_createSearchSelStart = 0;
            m_createSearchSelEnd = static_cast<int>(m_createSearchInput.length());
            m_createSearchCursor = m_createSearchSelEnd;
            return true;
        }

        // ctrl+c: copy
        if (isKeyC && isCtrl) {
            std::string toCopy;
            if (hasCreateSearchSelection()) {
                int s = std::min(m_createSearchSelStart, m_createSearchSelEnd);
                int e = std::max(m_createSearchSelStart, m_createSearchSelEnd);
                s = std::clamp(s, 0, static_cast<int>(m_createSearchInput.length()));
                e = std::clamp(e, 0, static_cast<int>(m_createSearchInput.length()));
                toCopy = m_createSearchInput.substr(s, e - s);
            } else {
                toCopy = m_createSearchInput;
            }
            if (!toCopy.empty()) {
                GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
                gtk_clipboard_set_text(clip, toCopy.c_str(), static_cast<gint>(toCopy.length()));
            }
            return true;
        }

        // ctrl+x: cut
        if (isKeyX && isCtrl) {
            if (hasCreateSearchSelection()) {
                int s = std::min(m_createSearchSelStart, m_createSearchSelEnd);
                int e = std::max(m_createSearchSelStart, m_createSearchSelEnd);
                s = std::clamp(s, 0, static_cast<int>(m_createSearchInput.length()));
                e = std::clamp(e, 0, static_cast<int>(m_createSearchInput.length()));
                std::string toCopy = m_createSearchInput.substr(s, e - s);
                if (!toCopy.empty()) {
                    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
                    gtk_clipboard_set_text(clip, toCopy.c_str(), static_cast<gint>(toCopy.length()));
                }
                deleteCreateSearchSelection();
            }
            return true;
        }

        // ctrl+v: paste (filters out any cyrillic characters)
        if (isKeyV && isCtrl) {
            GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            gchar* ptext = gtk_clipboard_wait_for_text(clip);
            if (ptext) {
                std::string s(ptext);
                s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
                s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());

                // strip out cyrillic characters
                std::string clean;
                const char* p = s.c_str();
                while (*p) {
                    gunichar c = g_utf8_get_char(p);
                    if (!isCyrillicCodePoint(c)) {
                        char buf[8] = {0};
                        int len = g_unichar_to_utf8(c, buf);
                        clean.append(buf, len);
                    }
                    p = g_utf8_next_char(p);
                }

                if (!clean.empty()) {
                    if (hasCreateSearchSelection()) {
                        deleteCreateSearchSelection();
                    }
                    m_createSearchInput.insert(m_createSearchCursor, clean);
                    m_createSearchCursor += static_cast<int>(clean.length());
                    clearCreateSearchSelection();
                }
                g_free(ptext);
            }
            return true;
        }

        // ctrl+backspace: delete previous word or token
        if (isKeyBackspace && isCtrl) {
            if (hasCreateSearchSelection()) {
                deleteCreateSearchSelection();
            } else if (m_createSearchCursor > 0 && !m_createSearchInput.empty()) {
                int pos = m_createSearchCursor;
                while (pos > 0 && isspace(static_cast<unsigned char>(m_createSearchInput[pos - 1]))) --pos;
                if (pos > 0) {
                    unsigned char c = static_cast<unsigned char>(m_createSearchInput[pos - 1]);
                    bool isWord = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
                    if (isWord) {
                        while (pos > 0) {
                            unsigned char ch = static_cast<unsigned char>(m_createSearchInput[pos - 1]);
                            bool w = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
                            if (!w) break;
                            --pos;
                        }
                    } else {
                        while (pos > 0) {
                            unsigned char ch = static_cast<unsigned char>(m_createSearchInput[pos - 1]);
                            bool w = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
                            if (w || isspace(ch)) break;
                            --pos;
                        }
                    }
                }
                while (pos > 0 && (static_cast<unsigned char>(m_createSearchInput[pos]) & 0xC0) == 0x80) --pos;
                m_createSearchInput.erase(pos, m_createSearchCursor - pos);
                m_createSearchCursor = pos;
            }
            return true;
        }

        // backspace: delete single char
        if (isKeyBackspace) {
            if (hasCreateSearchSelection()) {
                deleteCreateSearchSelection();
            } else if (m_createSearchCursor > 0 && !m_createSearchInput.empty()) {
                int prev = m_createSearchCursor - 1;
                while (prev > 0 && (static_cast<unsigned char>(m_createSearchInput[prev]) & 0xC0) == 0x80) {
                    --prev;
                }
                m_createSearchInput.erase(prev, m_createSearchCursor - prev);
                m_createSearchCursor = prev;
            }
            return true;
        }

        // delete: forward delete
        if (sym == SDLK_DELETE || sym == 0xffff || sym == 127) {
            if (hasCreateSearchSelection()) {
                deleteCreateSearchSelection();
            } else if (m_createSearchCursor < static_cast<int>(m_createSearchInput.length())) {
                int next = m_createSearchCursor + 1;
                while (next < static_cast<int>(m_createSearchInput.length()) &&
                       (static_cast<unsigned char>(m_createSearchInput[next]) & 0xC0) == 0x80) {
                    ++next;
                }
                m_createSearchInput.erase(m_createSearchCursor, next - m_createSearchCursor);
            }
            return true;
        }

        // left arrow (step over full utf-8 codepoint)
        if (sym == SDLK_LEFT || sym == 0xff51) {
            if (m_createSearchCursor > 0) {
                int prev = m_createSearchCursor - 1;
                while (prev > 0 && (static_cast<unsigned char>(m_createSearchInput[prev]) & 0xC0) == 0x80) {
                    --prev;
                }
                m_createSearchCursor = prev;
            }
            clearCreateSearchSelection();
            return true;
        }

        // right arrow (step over full utf-8 codepoint)
        if (sym == SDLK_RIGHT || sym == 0xff53) {
            if (m_createSearchCursor < static_cast<int>(m_createSearchInput.length())) {
                int next = m_createSearchCursor + 1;
                while (next < static_cast<int>(m_createSearchInput.length()) &&
                       (static_cast<unsigned char>(m_createSearchInput[next]) & 0xC0) == 0x80) {
                    ++next;
                }
                m_createSearchCursor = next;
            }
            clearCreateSearchSelection();
            return true;
        }

        // home
        if (sym == SDLK_HOME || sym == 0xff50) {
            m_createSearchCursor = 0;
            clearCreateSearchSelection();
            return true;
        }

        // end
        if (sym == SDLK_END || sym == 0xff57) {
            m_createSearchCursor = static_cast<int>(m_createSearchInput.length());
            clearCreateSearchSelection();
            return true;
        }

        // character input: strictly forbid cyrillic characters
        if (!isCtrl && text && text[0] != '\0') {
            unsigned char first = static_cast<unsigned char>(text[0]);
            if (first >= 32 && first != 127) {
                // reject if any character is cyrillic or if sym is cyrillic
                if (stringHasCyrillic(text) || isCyrillicCodePoint(sym)) {
                    return true;
                }
                if (hasCreateSearchSelection()) {
                    deleteCreateSearchSelection();
                }
                m_createSearchInput.insert(m_createSearchCursor, text);
                m_createSearchCursor += static_cast<int>(std::strlen(text));
                clearCreateSearchSelection();
                return true;
            }
        }
        return true;
    }

    if (sym == SDLK_ESCAPE || sym == 0xff1b || sym == 27) {
        if (m_searchEditMode) {
            m_searchEditMode = false;
            return true;
        }
        if (m_searchDropdownOpen) {
            m_searchDropdownOpen = false;
            return true;
        }
        setVisible(false);
        return true;
    }
    return true;
}

} // namespace Blueprint::UI
