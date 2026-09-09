#pragma once
#include <chrono>
#include <cstdint>

namespace Blueprint::Engine {

// Cubic-bezier(p1x, p1y, p2x, p2y) solver via Newton-Raphson
class CubicBezier {
public:
    constexpr CubicBezier(float p1x=0.16f,float p1y=1.f,float p2x=0.3f,float p2y=1.f)
        : m_x1(p1x), m_y1(p1y), m_x2(p2x), m_y2(p2y) {}

    float solve(float x, float eps = 1e-5f) const;

private:
    float m_x1, m_y1, m_x2, m_y2;
    float sampleX(float t) const {
        return ((1-3*m_x2+3*m_x1)*t + (3*m_x2-6*m_x1))*t*t + 3*m_x1*t;
    }
    float sampleY(float t) const {
        return ((1-3*m_y2+3*m_y1)*t + (3*m_y2-6*m_y1))*t*t + 3*m_y1*t;
    }
    float sampleDX(float t) const {
        return (3*(1-3*m_x2+3*m_x1)*t + 2*(3*m_x2-6*m_x1))*t + 3*m_x1;
    }
};

// Generic 0→1 animator (any duration, any bezier)
class Animator {
public:
    Animator() = default;
    Animator(float durationMs, CubicBezier curve = {0.16f,1.f,0.3f,1.f})
        : m_durationMs(durationMs), m_bezier(curve) {}

    void setDuration(float ms) { m_durationMs = ms; }
    void play(bool forward = true);
    void playForward() { play(true); }
    void playReverse() { play(false); }
    void setInstant(float v);

    void update();
    bool isRunning() const { return m_running; }
    float value() const { return m_value; }
    float rawProgress() const { return m_raw; }

private:
    float  m_durationMs = 180.f;
    CubicBezier m_bezier;
    bool   m_running = false;
    bool   m_forward = true;
    float  m_startValue = 0.f;
    float  m_raw = 0.f;
    float  m_value = 0.f;
    std::chrono::steady_clock::time_point m_startTime;
};

// Directional tab slide (reuses Animator internally)
class TabTransitionController {
public:
    TabTransitionController();

    void setSpeedMultiplier(float s) { m_speedMul = s; }
    void startTransition(int fromIndex, int toIndex);
    void update();

    bool  isTransitioning()  const { return m_animator.isRunning(); }
    float getProgress()      const { return m_animator.value(); }
    float getDirection()     const { return m_direction; }
    int   getFromIndex()     const { return m_fromIndex; }
    int   getToIndex()       const { return m_toIndex; }

private:
    int   m_fromIndex = 0;
    int   m_toIndex   = 0;
    float m_direction = 1.f;
    float m_speedMul  = 1.f;
    Animator m_animator;
};

} // namespace Blueprint::Engine
