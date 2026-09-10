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

    // 5. scoria (dark)
    // porous volcanic basalt with warm peat and deep geological ember glow
    Palette scoria;
    scoria.id          = ThemeId::SCORIA;
    scoria.name        = "scoria";
    scoria.displayName = "scoria";
    scoria.description = "Porous basalt anthracite with deep geological ember glow";
    scoria.isDark      = true;
    scoria.bgBase      = Color::fromHex(0x111012);
    scoria.bgSurface   = Color::fromHex(0x1A181C);
    scoria.border      = Color::fromHex(0x2E2A33);
    scoria.textPrimary = Color::fromHex(0xE8E2E6);
    scoria.accent      = Color::fromHex(0xE5935C);
    scoria.textMuted   = Color::fromHex(0x8E8594);
    scoria.bgSubtle    = Color::fromHex(0x221F25);
    scoria.bgActive    = Color::fromHex(0x2A262E);
    scoria.bgPopup     = Color::fromHex(0x1C191E);
    scoria.borderFocus = Color::fromHex(0x4A4153);
    scoria.textDim     = Color::fromHex(0x635B69);
    scoria.accentDim   = Color::fromHex(0x7A4826);
    scoria.danger      = Color::fromHex(0xE05560);
    m_palettes.push_back(scoria);

    // 6. stibnite (dark)
    // cold lead-steel antimonite crystal monochrome with razor focus
    Palette stibnite;
    stibnite.id          = ThemeId::STIBNITE;
    stibnite.name        = "stibnite";
    stibnite.displayName = "stibnite";
    stibnite.description = "Cold lead-steel monochrome with razor crystal electric beam";
    stibnite.isDark      = true;
    stibnite.bgBase      = Color::fromHex(0x0E1014);
    stibnite.bgSurface   = Color::fromHex(0x161920);
    stibnite.border      = Color::fromHex(0x262C38);
    stibnite.textPrimary = Color::fromHex(0xE4E8F0);
    stibnite.accent      = Color::fromHex(0x78A9FF);
    stibnite.textMuted   = Color::fromHex(0x737E94);
    stibnite.bgSubtle    = Color::fromHex(0x1D222B);
    stibnite.bgActive    = Color::fromHex(0x252C38);
    stibnite.bgPopup     = Color::fromHex(0x181B22);
    stibnite.borderFocus = Color::fromHex(0x3F4C61);
    stibnite.textDim     = Color::fromHex(0x525C6F);
    stibnite.accentDim   = Color::fromHex(0x365894);
    stibnite.danger      = Color::fromHex(0xE05560);
    m_palettes.push_back(stibnite);

    // 7. tephra (dark)
    // achromatic volcanic ash cloud with calming biological sage accent
    Palette tephra;
    tephra.id          = ThemeId::TEPHRA;
    tephra.name        = "tephra";
    tephra.displayName = "tephra";
    tephra.description = "Achromatic graphite ash cloud with calming dry sage accent";
    tephra.isDark      = true;
    tephra.bgBase      = Color::fromHex(0x121212);
    tephra.bgSurface   = Color::fromHex(0x1C1C1C);
    tephra.border      = Color::fromHex(0x2E2E2E);
    tephra.textPrimary = Color::fromHex(0xDEDEDE);
    tephra.accent      = Color::fromHex(0x9EC49E);
    tephra.textMuted   = Color::fromHex(0x7A7A7A);
    tephra.bgSubtle    = Color::fromHex(0x242424);
    tephra.bgActive    = Color::fromHex(0x2D2D2D);
    tephra.bgPopup     = Color::fromHex(0x1E1E1E);
    tephra.borderFocus = Color::fromHex(0x4A4A4A);
    tephra.textDim     = Color::fromHex(0x5A5A5A);
    tephra.accentDim   = Color::fromHex(0x4B6A4B);
    tephra.danger      = Color::fromHex(0xD45050);
    m_palettes.push_back(tephra);

    // 8. kaolin (light)
    // velvety raw porcelain clay with warm terracotta ceramic stamp
    Palette kaolin;
    kaolin.id          = ThemeId::KAOLIN;
    kaolin.name        = "kaolin";
    kaolin.displayName = "kaolin";
    kaolin.description = "Velvety raw porcelain clay with warm terracotta ceramic stamp";
    kaolin.isDark      = false;
    kaolin.bgBase      = Color::fromHex(0xF6F3ED);
    kaolin.bgSurface   = Color::fromHex(0xEBE6DC);
    kaolin.border      = Color::fromHex(0xD4CDBF);
    kaolin.textPrimary = Color::fromHex(0x24211D);
    kaolin.accent      = Color::fromHex(0xB5543C);
    kaolin.textMuted   = Color::fromHex(0x80776B);
    kaolin.bgSubtle    = Color::fromHex(0xDED7C9);
    kaolin.bgActive    = Color::fromHex(0xD3CBBA);
    kaolin.bgPopup     = Color::fromHex(0xEDE7DD);
    kaolin.borderFocus = Color::fromHex(0xA69A85);
    kaolin.textDim     = Color::fromHex(0x998F82);
    kaolin.accentDim   = Color::fromHex(0x733121);
    kaolin.danger      = Color::fromHex(0xC93636);
    m_palettes.push_back(kaolin);

    // 9. selenite (light)
    // milky pearlescent moonstone alabaster with noble mineral ultramarine
    Palette selenite;
    selenite.id          = ThemeId::SELENITE;
    selenite.name        = "selenite";
    selenite.displayName = "selenite";
    selenite.description = "Milky pearlescent moonstone with noble mineral ultramarine";
    selenite.isDark      = false;
    selenite.bgBase      = Color::fromHex(0xF1F0F5);
    selenite.bgSurface   = Color::fromHex(0xE5E3EC);
    selenite.border      = Color::fromHex(0xCBCEDB);
    selenite.textPrimary = Color::fromHex(0x1F1C2B);
    selenite.accent      = Color::fromHex(0x6052A8);
    selenite.textMuted   = Color::fromHex(0x77738A);
    selenite.bgSubtle    = Color::fromHex(0xDBD8E5);
    selenite.bgActive    = Color::fromHex(0xCFCBDD);
    selenite.bgPopup     = Color::fromHex(0xEAE7F2);
    selenite.borderFocus = Color::fromHex(0x9691B0);
    selenite.textDim     = Color::fromHex(0x938FA6);
    selenite.accentDim   = Color::fromHex(0x3D3370);
    selenite.danger      = Color::fromHex(0xC93B55);
    m_palettes.push_back(selenite);

    // 10. loess (light)
    // sun-bleached steppe loess soil with deep pine herbal tone
    Palette loess;
    loess.id          = ThemeId::LOESS;
    loess.name        = "loess";
    loess.displayName = "loess";
    loess.description = "Sun-bleached steppe loess soil with deep pine herbal tone";
    loess.isDark      = false;
    loess.bgBase      = Color::fromHex(0xF5F1E6);
    loess.bgSurface   = Color::fromHex(0xE8E2D1);
    loess.border      = Color::fromHex(0xCFC7B0);
    loess.textPrimary = Color::fromHex(0x26231A);
    loess.accent      = Color::fromHex(0x586E3F);
    loess.textMuted   = Color::fromHex(0x827B68);
    loess.bgSubtle    = Color::fromHex(0xDDD5BE);
    loess.bgActive    = Color::fromHex(0xD1C7AC);
    loess.bgPopup     = Color::fromHex(0xEDE6D3);
    loess.borderFocus = Color::fromHex(0x9C9378);
    loess.textDim     = Color::fromHex(0x968F7C);
    loess.accentDim   = Color::fromHex(0x374825);
    loess.danger      = Color::fromHex(0xC93636);
    m_palettes.push_back(loess);

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
