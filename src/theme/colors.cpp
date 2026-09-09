#include "theme/colors.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>

namespace Blueprint::Theme {

// static global live dynamic colors
Color BG_ABYSS     = Color::fromHex(0x0C1214);
Color BG_SURFACE   = Color::fromHex(0x141D22);
Color BG_SUBTLE    = Color::fromHex(0x1A252C);
Color BG_ACTIVE    = Color::fromHex(0x22323B);
Color BG_POPUP     = Color::fromHex(0x162026);
Color BORDER_SOFT  = Color::fromHex(0x24333B);
Color BORDER_FOCUS = Color::fromHex(0x3B5462);
Color TEXT_MAIN    = Color::fromHex(0xE1ECF0);
Color TEXT_MUTED   = Color::fromHex(0x6E8590);
Color TEXT_DIM     = Color::fromHex(0x475962);
Color ACCENT_CALM  = Color::fromHex(0x6CE5B9);
Color ACCENT_DIM   = Color::fromHex(0x2A624E);
Color DANGER       = Color::fromHex(0xE05060);

static inline Color lerpColor(const Color& a, const Color& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color{
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t
    };
}

ThemeManager& ThemeManager::instance() {
    static ThemeManager mgr;
    return mgr;
}

ThemeManager::ThemeManager() {
    // 1. noctiluca (dark)
    // deep oceanic teal darkness with cold bioluminescent phosphor glow
    Palette noctiluca;
    noctiluca.id          = ThemeId::NOCTILUCA;
    noctiluca.name        = "noctiluca";
    noctiluca.displayName = "noctiluca";
    noctiluca.description = "Deep carbon-teal oceanic darkness with bioluminescent glow";
    noctiluca.isDark      = true;
    noctiluca.bgBase      = Color::fromHex(0x0C1214);
    noctiluca.bgSurface   = Color::fromHex(0x141D22);
    noctiluca.border      = Color::fromHex(0x24333B);
    noctiluca.textPrimary = Color::fromHex(0xE1ECF0);
    noctiluca.accent      = Color::fromHex(0x6CE5B9);
    noctiluca.textMuted   = Color::fromHex(0x6E8590);
    noctiluca.bgSubtle    = Color::fromHex(0x19252C);
    noctiluca.bgActive    = Color::fromHex(0x22323B);
    noctiluca.bgPopup     = Color::fromHex(0x152026);
    noctiluca.borderFocus = Color::fromHex(0x3B5462);
    noctiluca.textDim     = Color::fromHex(0x475962);
    noctiluca.accentDim   = Color::fromHex(0x2A624E);
    noctiluca.danger      = Color::fromHex(0xE05060);
    m_palettes.push_back(noctiluca);

    // 2. morion (dark)
    // smoky quartz minerals with warm amber and volcanic stone tones
    Palette morion;
    morion.id          = ThemeId::MORION;
    morion.name        = "morion";
    morion.displayName = "morion";
    morion.description = "Smoky quartz minerals with warm amber crystal reflections";
    morion.isDark      = true;
    morion.bgBase      = Color::fromHex(0x131114);
    morion.bgSurface   = Color::fromHex(0x1D1920);
    morion.border      = Color::fromHex(0x342D38);
    morion.textPrimary = Color::fromHex(0xECE4EB);
    morion.accent      = Color::fromHex(0xDCA574);
    morion.textMuted   = Color::fromHex(0x98899A);
    morion.bgSubtle    = Color::fromHex(0x26202A);
    morion.bgActive    = Color::fromHex(0x312A36);
    morion.bgPopup     = Color::fromHex(0x1F1B22);
    morion.borderFocus = Color::fromHex(0x56495C);
    morion.textDim     = Color::fromHex(0x675969);
    morion.accentDim   = Color::fromHex(0x6F5036);
    morion.danger      = Color::fromHex(0xDF5B6D);
    m_palettes.push_back(morion);

    // 3. calcite (light)
    // mineral limestone and unbleached linen with gentle pine-jade accent
    Palette calcite;
    calcite.id          = ThemeId::CALCITE;
    calcite.name        = "calcite";
    calcite.displayName = "calcite";
    calcite.description = "Warm mineral limestone with unbleached linen & jade accent";
    calcite.isDark      = false;
    calcite.bgBase      = Color::fromHex(0xF4F2EA);
    calcite.bgSurface   = Color::fromHex(0xE8E5DB);
    calcite.border      = Color::fromHex(0xD0CBBE);
    calcite.textPrimary = Color::fromHex(0x201E1A);
    calcite.accent      = Color::fromHex(0x3B685C);
    calcite.textMuted   = Color::fromHex(0x858072);
    calcite.bgSubtle    = Color::fromHex(0xDCD7CB);
    calcite.bgActive    = Color::fromHex(0xD2CCBF);
    calcite.bgPopup     = Color::fromHex(0xEDE9DE);
    calcite.borderFocus = Color::fromHex(0x8B9E95);
    calcite.textDim     = Color::fromHex(0x9E988B);
    calcite.accentDim   = Color::fromHex(0x294941);
    calcite.danger      = Color::fromHex(0xD14354);
    m_palettes.push_back(calcite);

    // 4. rime (light)
    // crystal morning frost, stormy indigo contrast and cold tempered steel blue
    Palette rime;
    rime.id          = ThemeId::RIME;
    rime.name        = "rime";
    rime.displayName = "rime";
    rime.description = "Crystal clear frosty daylight with storm indigo and steel blue";
    rime.isDark      = false;
    rime.bgBase      = Color::fromHex(0xECF1F4);
    rime.bgSurface   = Color::fromHex(0xDFE6EB);
    rime.border      = Color::fromHex(0xC2CDD7);
    rime.textPrimary = Color::fromHex(0x182129);
    rime.accent      = Color::fromHex(0x2D638E);
    rime.textMuted   = Color::fromHex(0x728494);
    rime.bgSubtle    = Color::fromHex(0xD1DCE3);
    rime.bgActive    = Color::fromHex(0xC5D3DC);
    rime.bgPopup     = Color::fromHex(0xE5EDF2);
    rime.borderFocus = Color::fromHex(0x6894B8);
    rime.textDim     = Color::fromHex(0x92A3B0);
    rime.accentDim   = Color::fromHex(0x1E4361);
    rime.danger      = Color::fromHex(0xD44558);
    m_palettes.push_back(rime);

    // initialize with default noctiluca
    applyPaletteDirect(m_palettes[0]);
}

void ThemeManager::init() {
    if (m_initialized) return;
    m_initialized = true;

    // load saved theme from database
    std::string saved = Blueprint::Storage::Database::instance().getSetting("theme", "noctiluca");
    setThemeByName(saved, false);
}

const Palette& ThemeManager::getPalette(ThemeId id) const {
    for (const auto& p : m_palettes) {
        if (p.id == id) return p;
    }
    return m_palettes[0];
}

void ThemeManager::captureCurrentColors(Palette& p) const {
    p.bgBase      = BG_ABYSS;
    p.bgSurface   = BG_SURFACE;
    p.bgSubtle    = BG_SUBTLE;
    p.bgActive    = BG_ACTIVE;
    p.bgPopup     = BG_POPUP;
    p.border      = BORDER_SOFT;
    p.borderFocus = BORDER_FOCUS;
    p.textPrimary = TEXT_MAIN;
    p.textMuted   = TEXT_MUTED;
    p.textDim     = TEXT_DIM;
    p.accent      = ACCENT_CALM;
    p.accentDim   = ACCENT_DIM;
    p.danger      = DANGER;
}

void ThemeManager::applyPaletteDirect(const Palette& p) {
    m_currentTheme = p.id;
    BG_ABYSS     = p.bgBase;
    BG_SURFACE   = p.bgSurface;
    BG_SUBTLE    = p.bgSubtle;
    BG_ACTIVE    = p.bgActive;
    BG_POPUP     = p.bgPopup;
    BORDER_SOFT  = p.border;
    BORDER_FOCUS = p.borderFocus;
    TEXT_MAIN    = p.textPrimary;
    TEXT_MUTED   = p.textMuted;
    TEXT_DIM     = p.textDim;
    ACCENT_CALM  = p.accent;
    ACCENT_DIM   = p.accentDim;
    DANGER       = p.danger;
}

void ThemeManager::setTheme(ThemeId id, bool animated) {
    const Palette& target = getPalette(id);
    m_currentTheme = id;

    // persist to database
    Blueprint::Storage::Database::instance().setSetting("theme", target.name);

    if (!animated) {
        m_animating = false;
        m_animProgress = 1.0f;
        applyPaletteDirect(target);
        return;
    }

    // start smooth color transition
    captureCurrentColors(m_animFrom);
    m_animTo = target;
    m_animProgress = 0.0f;
    m_animating = true;
}

void ThemeManager::setThemeByName(const std::string& name, bool animated) {
    for (const auto& p : m_palettes) {
        if (p.name == name) {
            setTheme(p.id, animated);
            return;
        }
    }
    // fallback
    setTheme(ThemeId::NOCTILUCA, animated);
}

void ThemeManager::update(float dt) {
    if (!m_animating) return;

    m_animProgress += dt / m_animDuration;
    if (m_animProgress >= 1.0f) {
        m_animProgress = 1.0f;
        m_animating = false;
        applyPaletteDirect(m_animTo);
        return;
    }

    // smooth cubic ease-out curve
    float t = 1.0f - std::pow(1.0f - m_animProgress, 3.0f);

    BG_ABYSS     = lerpColor(m_animFrom.bgBase,      m_animTo.bgBase,      t);
    BG_SURFACE   = lerpColor(m_animFrom.bgSurface,   m_animTo.bgSurface,   t);
    BG_SUBTLE    = lerpColor(m_animFrom.bgSubtle,    m_animTo.bgSubtle,    t);
    BG_ACTIVE    = lerpColor(m_animFrom.bgActive,    m_animTo.bgActive,    t);
    BG_POPUP     = lerpColor(m_animFrom.bgPopup,     m_animTo.bgPopup,     t);
    BORDER_SOFT  = lerpColor(m_animFrom.border,      m_animTo.border,      t);
    BORDER_FOCUS = lerpColor(m_animFrom.borderFocus, m_animTo.borderFocus, t);
    TEXT_MAIN    = lerpColor(m_animFrom.textPrimary, m_animTo.textPrimary, t);
    TEXT_MUTED   = lerpColor(m_animFrom.textMuted,   m_animTo.textMuted,   t);
    TEXT_DIM     = lerpColor(m_animFrom.textDim,     m_animTo.textDim,     t);
    ACCENT_CALM  = lerpColor(m_animFrom.accent,      m_animTo.accent,      t);
    ACCENT_DIM   = lerpColor(m_animFrom.accentDim,   m_animTo.accentDim,   t);
    DANGER       = lerpColor(m_animFrom.danger,      m_animTo.danger,      t);
}

bool ThemeManager::isNightTime() {
    std::time_t now = std::time(nullptr);
    std::tm tmLocal{};
#if defined(_WIN32)
    localtime_s(&tmLocal, &now);
#else
    localtime_r(&now, &tmLocal);
#endif
    return isNightTimeForHour(tmLocal.tm_hour);
}

} // namespace Blueprint::Theme
