#pragma once

#include "ShaderManager.h"
#include <opencv2/opencv.hpp>
#include <string>

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(int viewportWidth, int viewportHeight);
    void shutdown();

    // Upload a new frame as a texture and draw it
    void uploadFrame(const cv::Mat& rgbFrame);
    void draw();

    void resize(int width, int height);

    // Expose the texture id so ImGui can display it if desired
    GLuint textureId() const { return m_textureId; }

    ShaderManager& shaderManager() { return m_shaderManager; }

    // Read back the back-buffer pixels (shader effect already applied).
    // Returns an RGB cv::Mat at viewport resolution, top-left origin.
    cv::Mat getRenderedFrame() const;

    int viewportWidth()  const { return m_viewportW; }
    int viewportHeight() const { return m_viewportH; }

    std::string lastError() const;

    // Metrics
    double lastUploadMs() const { return m_lastUploadMs; }
    double lastDrawMs()   const { return m_lastDrawMs; }

private:
    void setupQuad();

    GLuint  m_vao{0};
    GLuint  m_vbo{0};
    GLuint  m_ebo{0};
    GLuint  m_textureId{0};

    int     m_viewportW{0};
    int     m_viewportH{0};
    int     m_texWidth{0};
    int     m_texHeight{0};

    ShaderManager m_shaderManager;
    std::string   m_lastError;

    double  m_lastUploadMs{0.0};
    double  m_lastDrawMs{0.0};
};
