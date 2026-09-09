#include "engine/tab_transition.hpp"
#include <cmath>
#include <algorithm>

namespace Blueprint::Engine {

float CubicBezier::solve(float x, float eps) const {
    if (x <= 0.f) return 0.f;
    if (x >= 1.f) return 1.f;
    float t = x;
    for (int i = 0; i < 8; ++i) {
        float cx = sampleX(t) - x;
        if (std::fabs(cx) < eps) return sampleY(t);
        float dx = sampleDX(t);
        if (std::fabs(dx) < 1e-6f) break;
        t -= cx / dx;
    }
    // Bisection fallback
    float lo = 0.f, hi = 1.f;
    t = x;
    while (lo < hi) {
        float cx = sampleX(t);
        if (std::fabs(cx - x) < eps) return sampleY(t);
        if (x > cx) lo = t; else hi = t;
        t = (lo + hi) * 0.5f;
    }
    return sampleY(t);
}

void Animator::play(bool forward) {
    m_forward    = forward;
    m_startValue = m_value;
    m_startTime  = std::chrono::steady_clock::now();
    m_running    = true;
}

void Animator::setInstant(float v) {
    m_value = v;
    m_raw   = v;
    m_running = false;
}

void Animator::update() {
    if (!m_running) return;

    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - m_startTime).count() / 1000.f;

    float t = std::clamp(elapsed / m_durationMs, 0.f, 1.f);
    m_raw = m_forward ? t : 1.f - t;

    // We animate from startValue to target (0 if reverse, 1 if forward)
    float target = m_forward ? 1.f : 0.f;
    float startV = m_startValue;
    float easedT = m_bezier.solve(t);
    m_value = startV + (target - startV) * easedT;

    if (elapsed >= m_durationMs) {
        m_value   = target;
        m_raw     = target;
        m_running = false;
    }
}

TabTransitionController::TabTransitionController()
    : m_animator(180.f, CubicBezier(0.16f, 1.f, 0.3f, 1.f)) {}

void TabTransitionController::startTransition(int fromIndex, int toIndex) {
    if (fromIndex == toIndex) return;
    m_fromIndex = fromIndex;
    m_toIndex   = toIndex;
    m_direction = (toIndex > fromIndex) ? 1.f : -1.f;

    // Speed multiplier: 1.0 = 180ms, 2.0 = 90ms, 0.5 = 360ms
    float dur = 180.f / std::max(0.1f, m_speedMul);
    m_animator.setDuration(dur);
    m_animator.setInstant(0.f);
    m_animator.playForward();
}

void TabTransitionController::update() {
    m_animator.update();
}

} // namespace Blueprint::Engine
