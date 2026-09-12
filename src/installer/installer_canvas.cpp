#include "installer/installer_canvas.hpp"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <random>
#include <unistd.h>
#include <iostream>
#include <cstring>

namespace Blueprint::Installer {

static float randomFloat(float min, float max) {
    static std::mt19937 rng(1337);
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

static std::string resolveLottiePath(const std::string& filename) {
    const std::vector<std::string> prefixes = {
        "assets/",
        "../assets/",
        "/home/elliot/Проекты/Blueprint Browser/assets/",
        "./"
    };
    for (const auto& p : prefixes) {
        std::string full = p + filename;
        if (access(full.c_str(), R_OK) == 0) {
            return full;
        }
    }
    return "assets/" + filename;
}

InstallerCanvas::InstallerCanvas() {
    m_renderBuffer.resize(ANIM_SIZE * ANIM_SIZE, 0);
    initWidget();
    loadAnimations();
}

InstallerCanvas::~InstallerCanvas() {
    if (m_tickId && m_drawingArea) {
        if (GTK_IS_WIDGET(m_drawingArea)) {
            gtk_widget_remove_tick_callback(m_drawingArea, m_tickId);
        }
        m_tickId = 0;
        m_drawingArea = nullptr;
    }

    // Stop background pre-caching thread and wait for it
    m_stopPreload = true;
    if (m_preloadThread.joinable()) {
        m_preloadThread.join();
    }

    // Free cached background surface
    if (m_cachedBgSurface) {
        cairo_surface_destroy(m_cachedBgSurface);
        m_cachedBgSurface = nullptr;
    }

    // Free all pre-rendered Lottie frame surfaces
    for (auto& sc : m_animations) {
        for (auto* surf : sc.cachedFrames) {
            if (surf) {
                cairo_surface_destroy(surf);
            }
        }
        sc.cachedFrames.clear();
        sc.isCached.store(false);
    }
}

void InstallerCanvas::initWidget() {
    m_drawingArea = gtk_drawing_area_new();
    gtk_widget_set_size_request(m_drawingArea, 440, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(m_drawingArea), "installer-canvas");

    g_signal_connect(m_drawingArea, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        auto* self = static_cast<InstallerCanvas*>(user_data);
        self->m_drawingArea = nullptr;
        self->m_tickId = 0;
    }), this);

    g_signal_connect(m_drawingArea, "draw", G_CALLBACK(onDraw), this);

    m_tickId = gtk_widget_add_tick_callback(m_drawingArea, onTick, this, nullptr);
}

void InstallerCanvas::precacheAnimation(size_t index) {
    if (index >= m_animations.size()) return;
    auto& sc = m_animations[index];
    if (!sc.anim || sc.totalFrames == 0) return;

    std::vector<cairo_surface_t*> frames;
    frames.reserve(sc.totalFrames);

    for (size_t f = 0; f < sc.totalFrames; ++f) {
        if (m_stopPreload.load()) {
            for (auto* s : frames) {
                if (s) cairo_surface_destroy(s);
            }
            return;
        }

        cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ANIM_SIZE, ANIM_SIZE);
        unsigned char* data = cairo_image_surface_get_data(surf);
        int stride = cairo_image_surface_get_stride(surf);
        std::memset(data, 0, stride * ANIM_SIZE);

        rlottie::Surface rlSurf(reinterpret_cast<uint32_t*>(data), ANIM_SIZE, ANIM_SIZE, static_cast<size_t>(stride));
        {
            std::lock_guard<std::mutex> lock(m_renderMutex);
            sc.anim->renderSync(f, rlSurf);
        }
        cairo_surface_mark_dirty(surf);
        frames.push_back(surf);
    }

    sc.cachedFrames = std::move(frames);
    sc.isCached.store(true, std::memory_order_release);
}

void InstallerCanvas::loadAnimations() {
    m_animations.resize(5);
    const std::vector<std::string> files = {
        "wave.json",
        "star.json",
        "ringed_planet.json",
        "onion.json",
        "popper.json"
    };

    for (size_t i = 0; i < files.size(); ++i) {
        std::string path = resolveLottiePath(files[i]);
        auto anim = rlottie::Animation::loadFromFile(path);
        if (anim) {
            m_animations[i].totalFrames = anim->totalFrame();
            m_animations[i].fps = anim->frameRate() > 0 ? anim->frameRate() : 60.0;
            m_animations[i].currentFrame = 0.0f;
            m_animations[i].anim = std::move(anim);
        } else {
            std::cerr << "[InstallerCanvas] Warning: Could not load Lottie animation: " << path << std::endl;
        }
    }

    // Pre-cache Step 0 (wave.json) synchronously for immediate rock-solid 60 FPS on first frame
    precacheAnimation(0);

    // Pre-cache remaining steps (1, 2, 3, 4) in background worker thread
    m_preloadThread = std::thread([this]() {
        for (size_t i = 1; i < m_animations.size(); ++i) {
            if (m_stopPreload.load()) break;
            precacheAnimation(i);
        }
    });
}

void InstallerCanvas::setStep(int step) {
    if (step == m_currentStep) return;
    m_previousStep = m_currentStep;
    m_currentStep = step;
    m_transitionProgress = 0.0f;
    m_isTransitioning = true;

    // Reset frame timer for current animation so it starts cleanly from beginning
    if (step >= 0 && step < static_cast<int>(m_animations.size())) {
        m_animations[step].currentFrame = 0.0f;
    }

    if (m_currentStep == 4) {
        // Initial festive burst for deployment
        spawnConfettiBurst(45, 140.0f, 440.0f);
    }
    if (m_drawingArea) {
        gtk_widget_queue_draw(m_drawingArea);
    }
}

void InstallerCanvas::updateThemeColors(const std::string& bgHex, const std::string& accentHex, bool isDark) {
    m_isDarkTheme = isDark;
    auto parseHex = [](const std::string& hex, float& r, float& g, float& b) {
        if (hex.size() >= 7 && hex[0] == '#') {
            unsigned int val = 0;
            if (sscanf(hex.c_str() + 1, "%x", &val) == 1) {
                r = ((val >> 16) & 0xFF) / 255.0f;
                g = ((val >> 8) & 0xFF) / 255.0f;
                b = (val & 0xFF) / 255.0f;
            }
        }
    };
    parseHex(bgHex, m_bgR, m_bgG, m_bgB);
    parseHex(accentHex, m_accentR, m_accentG, m_accentB);
    m_bgDirty = true;
    if (m_drawingArea) {
        gtk_widget_queue_draw(m_drawingArea);
    }
}

gboolean InstallerCanvas::onTick(GtkWidget* widget, GdkFrameClock* clock, gpointer user_data) {
    auto* self = static_cast<InstallerCanvas*>(user_data);

    if (!gtk_widget_get_mapped(widget)) {
        return G_SOURCE_CONTINUE;
    }

    gint64 frameTime = gdk_frame_clock_get_frame_time(clock);
    if (self->m_lastFrameTime == 0) {
        self->m_lastFrameTime = frameTime;
    }

    float dt = static_cast<float>((frameTime - self->m_lastFrameTime) / 1000000.0);
    self->m_lastFrameTime = frameTime;

    if (dt < 0.0001f) {
        return G_SOURCE_CONTINUE;
    }

    // MANDATORY NUANCE: clamped_dt = std::min(dt, 0.033f)
    float clamped_dt = std::min(dt, 0.033f);

    self->update(clamped_dt);
    gtk_widget_queue_draw(widget);

    return G_SOURCE_CONTINUE;
}

void InstallerCanvas::spawnConfettiBurst(int count, float originX, float originY) {
    static const float COLORS[][3] = {
        { 0.22f, 0.74f, 0.97f }, // Sky blue
        { 0.96f, 0.62f, 0.04f }, // Amber
        { 0.94f, 0.25f, 0.37f }, // Coral Rose
        { 0.06f, 0.73f, 0.51f }, // Emerald
        { 0.66f, 0.37f, 0.97f }, // Violet
        { 1.00f, 1.00f, 1.00f }  // Crisp white
    };

    for (int i = 0; i < count; ++i) {
        ConfettiParticle p;
        p.x = originX + randomFloat(-10.0f, 10.0f);
        p.y = originY + randomFloat(-10.0f, 10.0f);

        // Angled explosion towards upper-right (45 deg +/- 25 deg)
        float speed = randomFloat(220.0f, 480.0f);
        float angleRad = randomFloat(-1.25f, -0.45f);
        p.vx = speed * std::cos(angleRad);
        p.vy = speed * std::sin(angleRad);

        p.angle = randomFloat(0.0f, 6.28f);
        p.vAngle = randomFloat(-8.0f, 8.0f);
        p.size = randomFloat(7.0f, 12.0f);
        p.isCircle = (randomFloat(0.0f, 1.0f) > 0.65f);
        p.life = 0.0f;
        p.maxLife = randomFloat(2.2f, 3.8f);

        int cIdx = rand() % 6;
        p.r = COLORS[cIdx][0];
        p.g = COLORS[cIdx][1];
        p.b = COLORS[cIdx][2];

        m_confetti.push_back(p);
    }
}

void InstallerCanvas::update(float dt) {
    m_time += dt;

    // Smooth scene transition timer (180 ms)
    if (m_isTransitioning) {
        m_transitionProgress += dt / TRANSITION_DURATION;
        if (m_transitionProgress >= 1.0f) {
            m_transitionProgress = 1.0f;
            m_isTransitioning = false;
        }
    }

    // Advance active Lottie animation frames
    for (size_t i = 0; i < m_animations.size(); ++i) {
        auto& sc = m_animations[i];
        if (sc.totalFrames == 0) continue;

        if (static_cast<int>(i) == m_currentStep || (m_isTransitioning && static_cast<int>(i) == m_previousStep)) {
            sc.currentFrame += static_cast<float>(dt * sc.fps);
            if (sc.currentFrame >= static_cast<float>(sc.totalFrames)) {
                sc.currentFrame = std::fmod(sc.currentFrame, static_cast<float>(sc.totalFrames));
            }
        }
    }

    // Step 4: Update celebratory confetti particles
    if (m_currentStep == 4 || (m_isTransitioning && m_previousStep == 4)) {
        m_confettiSpawnTimer += dt;
        if (m_confettiSpawnTimer >= 1.6f) {
            m_confettiSpawnTimer = 0.0f;
            spawnConfettiBurst(24, 140.0f, 440.0f);
        }

        for (auto& p : m_confetti) {
            p.vy += 380.0f * dt; // Gravity
            p.vx *= (1.0f - 0.35f * dt); // Drag
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.angle += p.vAngle * dt;
            p.life += dt;
        }

        m_confetti.erase(
            std::remove_if(m_confetti.begin(), m_confetti.end(), [](const ConfettiParticle& p) {
                return p.life >= p.maxLife || p.y > 720.0f;
            }),
            m_confetti.end()
        );
    } else {
        m_confetti.clear();
    }
}

void InstallerCanvas::bakeBackground(int width, int height) {
    if (m_cachedBgSurface) {
        cairo_surface_destroy(m_cachedBgSurface);
        m_cachedBgSurface = nullptr;
    }

    m_cachedBgSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cr = cairo_create(m_cachedBgSurface);

    double r = 24.0; // 24px bottom-left corner radius matching window

    cairo_save(cr);

    // 0. Clip canvas drawing to respect the bottom-left 24px window corner radius
    cairo_new_sub_path(cr);
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, width, 0);
    cairo_line_to(cr, width, height);
    cairo_line_to(cr, r, height);
    cairo_arc(cr, r, height - r, r, M_PI_2, M_PI);
    cairo_line_to(cr, 0, 0);
    cairo_close_path(cr);
    cairo_clip(cr);

    // 1. Matte background with subtle ambient vertical gradient
    cairo_pattern_t* bgPat = cairo_pattern_create_linear(0, 0, 0, height);
    if (m_isDarkTheme) {
        cairo_pattern_add_color_stop_rgba(bgPat, 0.0, m_bgR * 1.08f, m_bgG * 1.08f, m_bgB * 1.08f, 1.0);
        cairo_pattern_add_color_stop_rgba(bgPat, 1.0, m_bgR * 0.92f, m_bgG * 0.92f, m_bgB * 0.92f, 1.0);
    } else {
        cairo_pattern_add_color_stop_rgba(bgPat, 0.0, 0.985, 0.99, 0.995, 1.0);
        cairo_pattern_add_color_stop_rgba(bgPat, 1.0, 0.94, 0.955, 0.97, 1.0);
    }
    cairo_set_source(cr, bgPat);
    cairo_paint(cr);
    cairo_pattern_destroy(bgPat);

    // Subtle ambient radial glow behind animation
    cairo_pattern_t* glow = cairo_pattern_create_radial(width * 0.5, height * 0.45, 10.0, width * 0.5, height * 0.45, width * 0.65);
    float glowAlpha = m_isDarkTheme ? 0.09f : 0.06f;
    cairo_pattern_add_color_stop_rgba(glow, 0.0, m_accentR, m_accentG, m_accentB, glowAlpha);
    cairo_pattern_add_color_stop_rgba(glow, 1.0, m_accentR, m_accentG, m_accentB, 0.0);
    cairo_set_source(cr, glow);
    cairo_paint(cr);
    cairo_pattern_destroy(glow);

    // 2. Smooth right vertical divider line separating 40/60 split
    cairo_set_source_rgba(cr, m_isDarkTheme ? 1.0 : 0.0, m_isDarkTheme ? 1.0 : 0.0, m_isDarkTheme ? 1.0 : 0.0, m_isDarkTheme ? 0.08 : 0.06);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, width - 0.5, 0);
    cairo_line_to(cr, width - 0.5, height);
    cairo_stroke(cr);

    cairo_restore(cr);

    // 3. Subtle 1px outer contour along left edge and bottom-left 24px arc
    cairo_save(cr);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, m_isDarkTheme ? 1.0 : 0.06,
                              m_isDarkTheme ? 1.0 : 0.09,
                              m_isDarkTheme ? 1.0 : 0.16,
                              m_isDarkTheme ? 0.12 : 0.08);
    cairo_new_sub_path(cr);
    cairo_move_to(cr, 0.5, 0);
    cairo_line_to(cr, 0.5, height - r);
    cairo_arc_negative(cr, r, height - r, r - 0.5, M_PI, M_PI_2);
    cairo_line_to(cr, width, height - 0.5);
    cairo_stroke(cr);
    cairo_restore(cr);

    cairo_destroy(cr);

    m_cachedBgWidth = width;
    m_cachedBgHeight = height;
    m_bgDirty = false;
}

gboolean InstallerCanvas::onDraw(GtkWidget* widget, cairo_t* cr, gpointer user_data) {
    auto* self = static_cast<InstallerCanvas*>(user_data);

    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);

    // Ensure baked background surface is valid and up-to-date
    if (!self->m_cachedBgSurface || self->m_cachedBgWidth != width || self->m_cachedBgHeight != height || self->m_bgDirty) {
        self->bakeBackground(width, height);
    }

    // Blit pre-baked background surface
    cairo_set_source_surface(cr, self->m_cachedBgSurface, 0, 0);
    cairo_paint(cr);

    // Render scenes with smooth cubic Hermite cross-dissolve transition
    if (self->m_isTransitioning) {
        float t = std::clamp(self->m_transitionProgress, 0.0f, 1.0f);
        // Smoothstep cubic easing: 3t^2 - 2t^3
        float smoothT = t * t * (3.0f - 2.0f * t);

        float prevAlpha = 1.0f - smoothT;
        if (prevAlpha > 0.001f) {
            self->drawScene(cr, self->m_previousStep, prevAlpha, width, height);
        }
        float currAlpha = smoothT;
        if (currAlpha > 0.001f) {
            self->drawScene(cr, self->m_currentStep, currAlpha, width, height);
        }
    } else {
        self->drawScene(cr, self->m_currentStep, 1.0f, width, height);
    }

    return TRUE;
}

void InstallerCanvas::drawScene(cairo_t* cr, int step, float alpha, int width, int height) {
    drawLottieAnimation(cr, step, alpha, width, height);

    // If step 4, also render the confetti layer
    if (step == 4) {
        cairo_save(cr);
        for (const auto& p : m_confetti) {
            float fade = 1.0f;
            if (p.life > p.maxLife * 0.7f) {
                fade = 1.0f - (p.life - p.maxLife * 0.7f) / (p.maxLife * 0.3f);
            }
            float particleAlpha = alpha * std::clamp(fade, 0.0f, 1.0f);

            cairo_save(cr);
            cairo_translate(cr, p.x, p.y);
            cairo_rotate(cr, p.angle);
            cairo_set_source_rgba(cr, p.r, p.g, p.b, particleAlpha);

            if (p.isCircle) {
                cairo_arc(cr, 0, 0, p.size * 0.45, 0, 2 * M_PI);
                cairo_fill(cr);
            } else {
                cairo_rectangle(cr, -p.size * 0.5, -p.size * 0.3, p.size, p.size * 0.6);
                cairo_fill(cr);
            }
            cairo_restore(cr);
        }
        cairo_restore(cr);
    }
}

void InstallerCanvas::drawLottieAnimation(cairo_t* cr, int step, float alpha, int width, int height) {
    if (step < 0 || step >= static_cast<int>(m_animations.size())) return;
    auto& sc = m_animations[step];
    if (sc.totalFrames == 0) return;

    size_t frameNo = static_cast<size_t>(sc.currentFrame) % sc.totalFrames;
    int posX = (width - ANIM_SIZE) / 2;
    int posY = (height - ANIM_SIZE) / 2 - 15;

    // Fast-path: use ultra-fast pre-rendered Cairo image surface
    if (sc.isCached.load(std::memory_order_acquire) && frameNo < sc.cachedFrames.size()) {
        cairo_surface_t* frameSurf = sc.cachedFrames[frameNo];
        if (frameSurf) {
            cairo_save(cr);
            cairo_set_source_surface(cr, frameSurf, posX, posY);
            if (alpha < 0.999f) {
                cairo_paint_with_alpha(cr, alpha);
            } else {
                cairo_paint(cr);
            }
            cairo_restore(cr);
            return;
        }
    }

    // Fallback: On-demand synchronized render if frame is not pre-cached yet
    if (!sc.anim) return;
    std::lock_guard<std::mutex> lock(m_renderMutex);
    std::fill(m_renderBuffer.begin(), m_renderBuffer.end(), 0);

    rlottie::Surface surface(m_renderBuffer.data(), ANIM_SIZE, ANIM_SIZE, ANIM_SIZE * sizeof(uint32_t));
    sc.anim->renderSync(frameNo, surface);

    cairo_surface_t* cairoSurf = cairo_image_surface_create_for_data(
        reinterpret_cast<unsigned char*>(m_renderBuffer.data()),
        CAIRO_FORMAT_ARGB32,
        ANIM_SIZE, ANIM_SIZE,
        ANIM_SIZE * sizeof(uint32_t)
    );

    cairo_save(cr);
    cairo_set_source_surface(cr, cairoSurf, posX, posY);
    if (alpha < 0.999f) {
        cairo_paint_with_alpha(cr, alpha);
    } else {
        cairo_paint(cr);
    }
    cairo_restore(cr);

    cairo_surface_destroy(cairoSurf);
}

} // namespace Blueprint::Installer
