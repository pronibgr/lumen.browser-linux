#pragma once
#include <GL/glew.h>
#include <string>

namespace Blueprint::Graphics {

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    bool compile(const std::string& vertexSource, const std::string& fragmentSource);
    void use() const;
    void unbind() const;

    GLint getUniformLocation(const std::string& name) const;
    void setFloat(const std::string& name, float value) const;
    void setInt(const std::string& name, int value) const;
    void setVec2(const std::string& name, float x, float y) const;
    void setVec4(const std::string& name, float x, float y, float z, float w) const;

    GLuint id() const { return m_programId; }

private:
    GLuint m_programId = 0;
    GLuint compileShader(GLenum type, const std::string& source);
};

// Built-in Shaders
extern const char* DEFAULT_VERT_SHADER;
extern const char* DEFAULT_FRAG_SHADER;
extern const char* TAB_SLIDE_VERT_SHADER;
extern const char* TAB_SLIDE_FRAG_SHADER;

} // namespace Blueprint::Graphics
