#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Blueprint::Theme {

struct Color {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

    static constexpr Color fromHex(uint32_t hex, float alpha = 1.0f) {
        return Color{
            ((hex >> 16) & 0xFF) / 255.0f,
            ((hex >>  8) & 0xFF) / 255.0f,
            ((hex >>  0) & 0xFF) / 255.0f,
            alpha
        };
    }

    bool operator==(const Color& o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
};

enum class ThemeId {
    NOCTILUCA = 0,
    MORION    = 1,
    CALCITE   = 2,
    RIME      = 3
};

struct Palette {
    ThemeId     id;
    std::string name;        // "noctiluca", "morion", "calcite", "rime"
    std::string displayName; // "noctiluca", "morion", "calcite", "rime"
    std::string description; // subtitle / aesthetic note
    bool        isDark;

    // exact color values specified by user
    Color bgBase;
    Color bgSurface;
    Color border;
    Color textPrimary;
    Color accent;
    Color textMuted;

    // derived shades for smooth full-browser consistency
    Color bgSubtle;
    Color bgActive;
    Color bgPopup;
    Color borderFocus;
    Color textDim;
    Color accentDim;
    Color danger;
};

// live dynamic colors exported globally across the browser
// draw calls referencing these will dynamically reflect animated theme transitions
extern Color BG_ABYSS;
extern Color BG_SURFACE;
extern Color BG_SUBTLE;
extern Color BG_ACTIVE;
extern Color BG_POPUP;
extern Color BORDER_SOFT;
extern Color BORDER_FOCUS;
extern Color TEXT_MAIN;
extern Color TEXT_MUTED;
extern Color TEXT_DIM;
extern Color ACCENT_CALM;
extern Color ACCENT_DIM;
extern Color DANGER;

// theme management and smooth color interpolation engine
class ThemeManager {
public:
    static ThemeManager& instance();

    void init();
    void update(float dt);
    bool wantsRedraw() const { return m_animating; }

    ThemeId currentTheme() const { return m_currentTheme; }
    const Palette& activePalette() const { return getPalette(m_currentTheme); }
    const Palette& getPalette(ThemeId id) const;
    const std::vector<Palette>& allPalettes() const { return m_palettes; }

    void setTheme(ThemeId id, bool animated = true);
    void setThemeByName(const std::string& name, bool animated = true);

    static bool isNightTime();
    static bool isNightTimeForHour(int hour) {
        return (hour >= 20 || hour < 8);
    }
    static bool isLightTheme(ThemeId id) {
        return id == ThemeId::CALCITE || id == ThemeId::RIME;
    }

private:
    ThemeManager();
    void applyPaletteDirect(const Palette& p);
    void captureCurrentColors(Palette& p) const;

    std::vector<Palette> m_palettes;
    ThemeId m_currentTheme = ThemeId::NOCTILUCA;

    bool m_animating = false;
    float m_animProgress = 1.0f;
    float m_animDuration = 0.35f; // 350ms smooth transition
    Palette m_animFrom;
    Palette m_animTo;
    bool m_initialized = false;
};

// ── Geometry ──────────────────────────────────────────────────────────────────
inline constexpr float RADIUS_MENU   = 10.0f;
inline constexpr float RADIUS_BTN    = 6.0f;

// ── Topbar Layout ─────────────────────────────────────────────────────────────
// Row 1: Tab strip + nav buttons
// Row 2: Omnibox / URL bar
inline constexpr int ROW1_HEIGHT     = 46;   // tab strip row
inline constexpr int ROW2_HEIGHT     = 38;   // omnibox row
inline constexpr int TOPBAR_HEIGHT   = ROW1_HEIGHT + ROW2_HEIGHT; // 84px total

// Tab geometry
inline constexpr int TAB_HEIGHT      = 32;
inline constexpr int TAB_MAX_WIDTH   = 200;
inline constexpr int TAB_MIN_WIDTH   = 72;   // absolute minimum before scroll
inline constexpr int NAV_BTN_SIZE    = 26;
inline constexpr int OMNIBOX_HEIGHT  = 26;

} // namespace Blueprint::Theme
