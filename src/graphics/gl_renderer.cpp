#include "graphics/gl_renderer.hpp"
#include "theme/colors.hpp"
#include <iostream>

namespace Blueprint::Graphics {

GLRenderer::GLRenderer() {
}

GLRenderer::~GLRenderer() {
    if (m_uiTexture != 0) glDeleteTextures(1, &m_uiTexture);
    if (m_quadVbo != 0) glDeleteBuffers(1, &m_quadVbo);
    if (m_quadVao != 0) glDeleteVertexArrays(1, &m_quadVao);
}

void GLRenderer::setupQuad() {
    // Normalized Device Coordinates (NDC) fullscreen quad
    float vertices[] = {
        // Pos        // TexCoord (flipped Y for OpenGL texture coordinates)
        -1.0f,  1.0f,  0.0f, 0.0f,
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,

        -1.0f,  1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 0.0f
    };

    glGenVertexArrays(1, &m_quadVao);
    glGenBuffers(1, &m_quadVbo);

    glBindVertexArray(m_quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // TexCoord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

bool GLRenderer::initialize(int windowWidth, int windowHeight) {
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "[OpenGL] GLEW Init failed: " << glewGetErrorString(err) << std::endl;
        return false;
    }

    m_width = windowWidth;
    m_height = windowHeight;

    // Compile Shaders
    if (!m_defaultShader.compile(DEFAULT_VERT_SHADER, DEFAULT_FRAG_SHADER)) {
        std::cerr << "[OpenGL] Default shader compilation failed\n";
        return false;
    }

    if (!m_tabSlideShader.compile(TAB_SLIDE_VERT_SHADER, TAB_SLIDE_FRAG_SHADER)) {
        std::cerr << "[OpenGL] Tab slide shader compilation failed\n";
        return false;
    }

    setupQuad();

    // Gen UI overlay texture
    glGenTextures(1, &m_uiTexture);
    glBindTexture(GL_TEXTURE_2D, m_uiTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return true;
}

void GLRenderer::resize(int windowWidth, int windowHeight) {
    m_width = windowWidth;
    m_height = windowHeight;
    glViewport(0, 0, windowWidth, windowHeight);
}

void GLRenderer::beginFrame() {
    // Clear with Deep Obsidian background #0E1116
    glClearColor(Theme::BG_ABYSS.r, Theme::BG_ABYSS.g, Theme::BG_ABYSS.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void GLRenderer::renderTabContent(const std::shared_ptr<Engine::WebTab>& currentTab,
                                  const std::shared_ptr<Engine::WebTab>& targetTab,
                                  const Engine::TabTransitionController& transition,
                                  int topbarHeight) {
    int contentHeight = m_height - topbarHeight;
    if (contentHeight <= 0 || m_width <= 0) return;

    // Viewport for web content (below topbar)
    glViewport(0, 0, m_width, contentHeight);

    glBindVertexArray(m_quadVao);

    if (transition.isTransitioning() && currentTab && targetTab) {
        // 180ms Directional Tab Slide GPU shader transition
        m_tabSlideShader.use();
        m_tabSlideShader.setFloat("u_progress", transition.getProgress());
        m_tabSlideShader.setFloat("u_direction", transition.getDirection());

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentTab->getTexture());
        m_tabSlideShader.setInt("u_textureOld", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, targetTab->getTexture());
        m_tabSlideShader.setInt("u_textureNew", 1);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        m_tabSlideShader.unbind();
    } else {
        auto activeTab = targetTab ? targetTab : currentTab;
        if (activeTab) {
            // Static active tab rendering
            m_defaultShader.use();
            m_defaultShader.setVec4("u_tintColor", 1.0f, 1.0f, 1.0f, 1.0f);
            m_defaultShader.setFloat("u_opacity", 1.0f);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, activeTab->getTexture());
            m_defaultShader.setInt("u_texture", 0);

            glDrawArrays(GL_TRIANGLES, 0, 6);
            m_defaultShader.unbind();
        }
    }

    glBindVertexArray(0);
}

void GLRenderer::renderUIOverlay(cairo_surface_t* uiSurface) {
    if (!uiSurface) return;

    // Full window viewport for UI overlay
    glViewport(0, 0, m_width, m_height);

    cairo_surface_flush(uiSurface);
    int surfW = cairo_image_surface_get_width(uiSurface);
    int surfH = cairo_image_surface_get_height(uiSurface);
    unsigned char* data = cairo_image_surface_get_data(uiSurface);

    glBindTexture(GL_TEXTURE_2D, m_uiTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surfW, surfH, 0, GL_BGRA, GL_UNSIGNED_BYTE, data);

    m_defaultShader.use();
    m_defaultShader.setVec4("u_tintColor", 1.0f, 1.0f, 1.0f, 1.0f);
    m_defaultShader.setFloat("u_opacity", 1.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_uiTexture);
    m_defaultShader.setInt("u_texture", 0);

    glBindVertexArray(m_quadVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    m_defaultShader.unbind();
}

void GLRenderer::endFrame() {
    // Swap buffer handled by SDL/EGL in main loop
}

} // namespace Blueprint::Graphics
