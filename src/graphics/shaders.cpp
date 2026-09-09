#include "graphics/shaders.hpp"
#include <iostream>
#include <vector>

namespace Blueprint::Graphics {

const char* DEFAULT_VERT_SHADER = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* DEFAULT_FRAG_SHADER = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D u_texture;
uniform vec4 u_tintColor;
uniform float u_opacity;

void main() {
    vec4 texColor = texture(u_texture, TexCoord);
    FragColor = texColor * u_tintColor * u_opacity;
}
)";

const char* TAB_SLIDE_VERT_SHADER = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

// Directional Tab Slide Shader
// u_direction: +1.0 (moving left/target > current), -1.0 (moving right/target < current)
// u_progress: 0.0 -> 1.0 (evaluated by cubic-bezier)
const char* TAB_SLIDE_FRAG_SHADER = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D u_textureOld;
uniform sampler2D u_textureNew;
uniform float u_progress;
uniform float u_direction; // +1.0 = Target to right (content slides left), -1.0 = Target to left

void main() {
    float shift = u_progress * u_direction;
    
    // UV offset for old texture (sliding out)
    vec2 uvOld = TexCoord + vec2(shift, 0.0);
    
    // UV offset for new texture (sliding in)
    vec2 uvNew = TexCoord - vec2((1.0 - u_progress) * u_direction, 0.0);
    
    vec4 colorOld = vec4(0.0);
    vec4 colorNew = vec4(0.0);
    
    if (uvOld.x >= 0.0 && uvOld.x <= 1.0 && uvOld.y >= 0.0 && uvOld.y <= 1.0) {
        colorOld = texture(u_textureOld, uvOld);
    }
    
    if (uvNew.x >= 0.0 && uvNew.x <= 1.0 && uvNew.y >= 0.0 && uvNew.y <= 1.0) {
        colorNew = texture(u_textureNew, uvNew);
    }
    
    // Smooth cross-dissolve edge blend during slide
    float edgeFactor = clamp((TexCoord.x - (1.0 - u_progress)) * 20.0, 0.0, 1.0);
    
    if (u_direction > 0.0) {
        FragColor = (TexCoord.x > 1.0 - u_progress) ? colorNew : colorOld;
    } else {
        FragColor = (TexCoord.x < u_progress) ? colorNew : colorOld;
    }
}
)";

ShaderProgram::~ShaderProgram() {
    if (m_programId != 0) {
        glDeleteProgram(m_programId);
    }
}

GLuint ShaderProgram::compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> infoLog(logLen);
        glGetShaderInfoLog(shader, logLen, nullptr, infoLog.data());
        std::cerr << "[Shader Error] Compilation failed:\n" << infoLog.data() << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool ShaderProgram::compile(const std::string& vertexSource, const std::string& fragmentSource) {
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (!vert) return false;

    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!frag) {
        glDeleteShader(vert);
        return false;
    }

    m_programId = glCreateProgram();
    glAttachShader(m_programId, vert);
    glAttachShader(m_programId, frag);
    glLinkProgram(m_programId);

    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint success = 0;
    glGetProgramiv(m_programId, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLen = 0;
        glGetProgramiv(m_programId, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> infoLog(logLen);
        glGetProgramInfoLog(m_programId, logLen, nullptr, infoLog.data());
        std::cerr << "[Shader Error] Linking failed:\n" << infoLog.data() << std::endl;
        glDeleteProgram(m_programId);
        m_programId = 0;
        return false;
    }
    return true;
}

void ShaderProgram::use() const {
    if (m_programId != 0) glUseProgram(m_programId);
}

void ShaderProgram::unbind() const {
    glUseProgram(0);
}

GLint ShaderProgram::getUniformLocation(const std::string& name) const {
    return glGetUniformLocation(m_programId, name.c_str());
}

void ShaderProgram::setFloat(const std::string& name, float value) const {
    glUniform1f(getUniformLocation(name), value);
}

void ShaderProgram::setInt(const std::string& name, int value) const {
    glUniform1i(getUniformLocation(name), value);
}

void ShaderProgram::setVec2(const std::string& name, float x, float y) const {
    glUniform2f(getUniformLocation(name), x, y);
}

void ShaderProgram::setVec4(const std::string& name, float x, float y, float z, float w) const {
    glUniform4f(getUniformLocation(name), x, y, z, w);
}

} // namespace Blueprint::Graphics
