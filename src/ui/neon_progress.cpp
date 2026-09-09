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
    m_targetProgress = std::clamp(progress, 0.0f, 1.0f);
    if (m_targetProgress >= 1.0f) {
        m_visible = false;
    } else {
        m_visible = true;
    }
}

void NeonProgress::update(float dt) {
    if (!m_visible) return;

    // Smooth interpolation to target progress
    m_currentProgress += (m_targetProgress - m_currentProgress) * std::min(1.0f, dt * 12.0f);

    m_glowPulse += dt * 5.0f;
    if (m_glowPulse > 2.0f * M_PI) {
        m_glowPulse -= 2.0f * M_PI;
    }
}

void NeonProgress::draw(cairo_t* cr, double x, double y, double width) {
    if (!m_visible || m_currentProgress <= 0.001f) return;

    double progressWidth = width * m_currentProgress;

    // Soft outer neon glow
    float pulseAlpha = 0.25f + 0.15f * std::sin(m_glowPulse);
    cairo_set_source_rgba(cr, Theme::ACCENT_CALM.r, Theme::ACCENT_CALM.g, Theme::ACCENT_CALM.b, pulseAlpha);
    cairo_set_line_width(cr, 4.0);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + progressWidth, y);
    cairo_stroke(cr);

    // Sharp 2px Core Neon Line
    cairo_set_source_rgb(cr, Theme::ACCENT_CALM.r, Theme::ACCENT_CALM.g, Theme::ACCENT_CALM.b);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + progressWidth, y);
    cairo_stroke(cr);
}

} // namespace Blueprint::UI
