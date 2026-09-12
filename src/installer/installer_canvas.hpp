#pragma once

#include <gtk/gtk.h>
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <rlottie.h>

namespace Blueprint::Installer {

struct ConfettiParticle {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float angle = 0.0f;
    float vAngle = 0.0f;
    float r = 1.0f, g = 1.0f, b = 1.0f;
    float size = 8.0f;
    bool isCircle = false;
    float life = 0.0f;
    float maxLife = 3.0f;
};

struct LottieScene {
    std::unique_ptr<rlottie::Animation> anim;
    size_t totalFrames = 0;
    double fps = 60.0;
    float currentFrame = 0.0f;
    std::vector<cairo_surface_t*> cachedFrames;
    std::atomic<bool> isCached{false};

    LottieScene() = default;
    LottieScene(LottieScene&& other) noexcept
        : anim(std::move(other.anim)),
          totalFrames(other.totalFrames),
          fps(other.fps),
          currentFrame(other.currentFrame),
          cachedFrames(std::move(other.cachedFrames)),
          isCached(other.isCached.load()) {}

    LottieScene& operator=(LottieScene&& other) noexcept {
        if (this != &other) {
            anim = std::move(other.anim);
            totalFrames = other.totalFrames;
            fps = other.fps;
            currentFrame = other.currentFrame;
            cachedFrames = std::move(other.cachedFrames);
            isCached.store(other.isCached.load());
        }
        return *this;
    }

    LottieScene(const LottieScene&) = delete;
    LottieScene& operator=(const LottieScene&) = delete;
};

class InstallerCanvas {
public:
    InstallerCanvas();
    ~InstallerCanvas();

    GtkWidget* getWidget() const { return m_drawingArea; }

    void setStep(int step);
    int getCurrentStep() const { return m_currentStep; }
    void updateThemeColors(const std::string& bgHex, const std::string& accentHex, bool isDark);

private:
    void initWidget();
    void loadAnimations();
    void precacheAnimation(size_t index);
    void bakeBackground(int width, int height);

    static gboolean onTick(GtkWidget* widget, GdkFrameClock* clock, gpointer user_data);
    static gboolean onDraw(GtkWidget* widget, cairo_t* cr, gpointer user_data);

    void update(float dt);
    void drawScene(cairo_t* cr, int step, float alpha, int width, int height);
    void drawLottieAnimation(cairo_t* cr, int step, float alpha, int width, int height);

    void spawnConfettiBurst(int count, float originX, float originY);

    GtkWidget* m_drawingArea = nullptr;
    guint m_tickId = 0;
    gint64 m_lastFrameTime = 0;

    int m_currentStep = 0;
    int m_previousStep = 0;
    bool m_isTransitioning = false;
    float m_transitionProgress = 1.0f;
    static constexpr float TRANSITION_DURATION = 0.180f; // 180ms dissolve

    float m_time = 0.0f;

    // Theme tinting
    float m_bgR = 0.97f, m_bgG = 0.98f, m_bgB = 0.99f;
    float m_accentR = 0.22f, m_accentG = 0.74f, m_accentB = 0.97f;
    bool m_isDarkTheme = false;

    // Background surface cache
    cairo_surface_t* m_cachedBgSurface = nullptr;
    int m_cachedBgWidth = 0;
    int m_cachedBgHeight = 0;
    bool m_bgDirty = true;

    // 5 Lottie animations for steps 0-4
    std::vector<LottieScene> m_animations;
    static constexpr int ANIM_SIZE = 340;
    std::vector<uint32_t> m_renderBuffer;
    std::thread m_preloadThread;
    std::atomic<bool> m_stopPreload{false};
    std::mutex m_renderMutex;

    // Festive confetti for Step 4 (Deploy)
    std::vector<ConfettiParticle> m_confetti;
    float m_confettiSpawnTimer = 0.0f;
};

} // namespace Blueprint::Installer

