#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "ui/neon_progress.hpp"
#include "theme/colors.hpp"
#include <algorithm>

namespace Blueprint::UI {

void NeonProgress::setProgress(float progress) {
    float p = std::clamp(progress, 0.0f, 1.0f);
    m_targetProgress = p;
    if (p >= 1.0f) {
        m_isFadingOut = true;
    } else {
        m_visible = true;
        m_isFadingOut = false;
        m_fadeAlpha = 1.0f;
    }
}

bool NeonProgress::wantsRedraw() const {
    return m_visible && (m_currentProgress < 0.999f || m_fadeAlpha > 0.01f || std::abs(m_targetProgress - m_currentProgress) > 0.002f);
}

void NeonProgress::update(float dt) {
    if (!m_visible) return;

    // Smooth framerate-independent interpolation to target progress
    float factor = 1.0f - std::exp(-18.0f * dt);
    m_currentProgress += (m_targetProgress - m_currentProgress) * factor;

    if (m_isFadingOut) {
        // Once progress has nearly reached 1.0, fade out smoothly
        if (m_currentProgress >= 0.98f) {
            m_fadeAlpha -= dt * 5.0f; // ~200ms smooth fadeout
            if (m_fadeAlpha <= 0.01f) {
                m_visible = false;
                m_isFadingOut = false;
                m_fadeAlpha = 1.0f;
                m_currentProgress = 0.0f;
                m_targetProgress = 0.0f;
            }
        }
    }

    m_glowPulse += dt * 6.0f;
    if (m_glowPulse > 2.0f * M_PI) {
        m_glowPulse -= 2.0f * M_PI;
    }
}

void NeonProgress::draw(cairo_t* cr, double x, double y, double width) {
    if (!m_visible || m_currentProgress <= 0.001f || m_fadeAlpha <= 0.01f) return;

    double progressWidth = width * m_currentProgress;

    // Soft outer neon glow
    float pulseAlpha = (0.25f + 0.15f * std::sin(m_glowPulse)) * m_fadeAlpha;
    cairo_set_source_rgba(cr, Theme::ACCENT_CALM.r, Theme::ACCENT_CALM.g, Theme::ACCENT_CALM.b, pulseAlpha);
    cairo_set_line_width(cr, 4.0);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + progressWidth, y);
    cairo_stroke(cr);

    // Sharp 2px Core Neon Line
    cairo_set_source_rgba(cr, Theme::ACCENT_CALM.r, Theme::ACCENT_CALM.g, Theme::ACCENT_CALM.b, m_fadeAlpha);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + progressWidth, y);
    cairo_stroke(cr);
}

} // namespace Blueprint::UI
