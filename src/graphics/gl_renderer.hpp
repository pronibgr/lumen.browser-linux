#pragma once
#include <GL/glew.h>
#include <cairo/cairo.h>
#include <memory>
#include "graphics/shaders.hpp"
#include "engine/tab_transition.hpp"
#include "engine/web_tab.hpp"

namespace Blueprint::Graphics {

class GLRenderer {
public:
    GLRenderer();
    ~GLRenderer();

    bool initialize(int windowWidth, int windowHeight);
    void resize(int windowWidth, int windowHeight);

    void beginFrame();
    void renderTabContent(const std::shared_ptr<Engine::WebTab>& currentTab,
                          const std::shared_ptr<Engine::WebTab>& targetTab,
                          const Engine::TabTransitionController& transition,
                          int topbarHeight);
    void renderUIOverlay(cairo_surface_t* uiSurface);
    void endFrame();

private:
    int m_width = 0;
    int m_height = 0;

    ShaderProgram m_defaultShader;
    ShaderProgram m_tabSlideShader;

    GLuint m_quadVao = 0;
    GLuint m_quadVbo = 0;
    GLuint m_uiTexture = 0;

    void setupQuad();
};

} // namespace Blueprint::Graphics
