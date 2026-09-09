#pragma once
#include <cairo/cairo.h>

namespace Blueprint::UI {

class NeonProgress {
public:
    NeonProgress() = default;

    void setProgress(float progress);
    float getProgress() const { return m_currentProgress; }
    void setVisible(bool visible) { m_visible = visible; }
    bool isVisible() const { return m_visible; }
    bool wantsRedraw() const;

    void update(float dt);
    void draw(cairo_t* cr, double x, double y, double width);

private:
    float m_targetProgress = 0.0f;
    float m_currentProgress = 0.0f;
    bool  m_visible = false;
    bool  m_isFadingOut = false;
    float m_fadeAlpha = 1.0f;
    float m_glowPulse = 0.0f;
};

} // namespace Blueprint::UI
