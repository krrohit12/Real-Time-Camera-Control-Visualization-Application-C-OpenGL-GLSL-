#include "ShaderManager.h"
#include <fstream>
#include <sstream>
#include <iostream>

ShaderManager::ShaderManager() = default;

ShaderManager::~ShaderManager() {
    for (auto& e : m_effects) {
        if (e.program) glDeleteProgram(e.program);
    }
    if (m_vertShader) glDeleteShader(m_vertShader);
}

bool ShaderManager::init(const std::string& shaderDir) {
    m_shaderDir = shaderDir;

    std::string vertSrc = loadFile(shaderDir + "/passthrough.vert");
    if (vertSrc.empty()) {
        m_lastError = "Could not load vertex shader: " + shaderDir + "/passthrough.vert";
        return false;
    }

    m_vertShader = compileShader(GL_VERTEX_SHADER, vertSrc);
    if (!m_vertShader) return false;

    // Each entry: { display name, fragment shader filename }
    const std::vector<std::pair<std::string,std::string>> effectDefs = {
        {"Normal",              "normal.frag"},
        {"Grayscale",           "grayscale.frag"},
        {"Sepia",               "sepia.frag"},
        {"Edge Detection",      "edge_detection.frag"},
        {"Blur",                "blur.frag"},
        {"Brightness/Contrast", "brightness_contrast.frag"},
    };

    for (auto& [name, file] : effectDefs) {
        std::string fragSrc = loadFile(shaderDir + "/" + file);
        if (fragSrc.empty()) {
            std::cerr << "[ShaderManager] Skipping " << name << ": " << file << " not found\n";
            continue;
        }
        GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
        if (!frag) continue;

        GLuint prog = linkProgram(m_vertShader, frag);
        glDeleteShader(frag);
        if (!prog) continue;

        m_effects.push_back({name, shaderDir + "/" + file, prog});
    }

    return !m_effects.empty();
}

bool ShaderManager::hasEffects() const { return !m_effects.empty(); }

const std::vector<ShaderEffect>& ShaderManager::effects() const { return m_effects; }

int ShaderManager::activeEffectIndex() const { return m_activeIndex; }

bool ShaderManager::setActiveEffect(int index) {
    if (index < 0 || index >= static_cast<int>(m_effects.size())) return false;
    m_activeIndex = index;
    return true;
}

GLuint ShaderManager::activeProgram() const {
    if (m_effects.empty()) return 0;
    return m_effects[m_activeIndex].program;
}

void ShaderManager::setFloat(const std::string& name, float value) {
    GLuint prog = activeProgram();
    if (!prog) return;
    GLint loc = glGetUniformLocation(prog, name.c_str());
    if (loc >= 0) glUniform1f(loc, value);
}

void ShaderManager::setInt(const std::string& name, int value) {
    GLuint prog = activeProgram();
    if (!prog) return;
    GLint loc = glGetUniformLocation(prog, name.c_str());
    if (loc >= 0) glUniform1i(loc, value);
}

void ShaderManager::setVec2(const std::string& name, float x, float y) {
    GLuint prog = activeProgram();
    if (!prog) return;
    GLint loc = glGetUniformLocation(prog, name.c_str());
    if (loc >= 0) glUniform2f(loc, x, y);
}

std::string ShaderManager::lastError() const { return m_lastError; }

// ── private ──────────────────────────────────────────────────────────────────

GLuint ShaderManager::compileShader(GLenum type, const std::string& src) {
    GLuint shader = glCreateShader(type);
    const char* srcPtr = src.c_str();
    glShaderSource(shader, 1, &srcPtr, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        m_lastError = std::string("Shader compile error: ") + log;
        std::cerr << m_lastError << "\n";
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint ShaderManager::linkProgram(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        m_lastError = std::string("Shader link error: ") + log;
        std::cerr << m_lastError << "\n";
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

std::string ShaderManager::loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return {};
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}
