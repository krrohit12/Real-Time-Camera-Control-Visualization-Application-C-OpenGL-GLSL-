#pragma once

#include <glad/glad.h>
#include <string>
#include <unordered_map>
#include <vector>

struct ShaderEffect {
    std::string name;
    std::string fragPath;
    GLuint      program{0};
};

class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();

    bool init(const std::string& shaderDir);

    // Returns false if no effects were loaded
    bool hasEffects() const;

    const std::vector<ShaderEffect>& effects() const;
    int  activeEffectIndex() const;
    bool setActiveEffect(int index);

    GLuint activeProgram() const;

    // Per-effect uniform setters
    void setFloat(const std::string& name, float value);
    void setInt  (const std::string& name, int   value);
    void setVec2 (const std::string& name, float x, float y);

    std::string lastError() const;

private:
    GLuint compileShader(GLenum type, const std::string& src);
    GLuint linkProgram(GLuint vert, GLuint frag);
    std::string loadFile(const std::string& path);

    std::vector<ShaderEffect> m_effects;
    int                       m_activeIndex{0};
    std::string               m_lastError;
    std::string               m_shaderDir;
    GLuint                    m_vertShader{0};  // shared vertex shader
};
