#pragma once
#include <cstdint>

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
};

// ── Deep Obsidian ─────────────────────────────────────────────────────────────
inline constexpr Color BG_ABYSS     = Color::fromHex(0x0E1116);
inline constexpr Color BG_SURFACE   = Color::fromHex(0x151A21);
inline constexpr Color BG_SUBTLE    = Color::fromHex(0x1D232C);
inline constexpr Color BG_ACTIVE    = Color::fromHex(0x28313D);
inline constexpr Color BG_POPUP     = Color::fromHex(0x19202A);
inline constexpr Color BORDER_SOFT  = Color::fromHex(0x2D3745);
inline constexpr Color BORDER_FOCUS = Color::fromHex(0x4A6080);
inline constexpr Color TEXT_MAIN    = Color::fromHex(0xC2CBD6);
inline constexpr Color TEXT_MUTED   = Color::fromHex(0x6C798C);
inline constexpr Color TEXT_DIM     = Color::fromHex(0x3D4A5C);
inline constexpr Color ACCENT_CALM  = Color::fromHex(0x5EEAD4);
inline constexpr Color ACCENT_DIM   = Color::fromHex(0x2A6B62);
inline constexpr Color DANGER       = Color::fromHex(0xE05060);

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
