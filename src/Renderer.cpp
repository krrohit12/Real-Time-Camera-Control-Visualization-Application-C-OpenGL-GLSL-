#include "Renderer.h"
#include <chrono>
#include <iostream>

// Full-screen quad: position (xy) + texcoord (uv)
// Texture coords are flipped vertically because OpenCV origin is top-left
// but OpenGL texture origin is bottom-left.
static const float kQuadVertices[] = {
    // positions   // texcoords
    -1.0f,  1.0f,  0.0f, 0.0f,
    -1.0f, -1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 1.0f,
     1.0f,  1.0f,  1.0f, 0.0f,
};

static const unsigned int kQuadIndices[] = { 0, 1, 2,  0, 2, 3 };

Renderer::Renderer() = default;

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init(int viewportWidth, int viewportHeight) {
    m_viewportW = viewportWidth;
    m_viewportH = viewportHeight;

    // Set the viewport explicitly so it matches the physical framebuffer.
    // Without this the default viewport is whatever GLFW set on context
    // creation, which may differ from our stored dimensions on Retina displays.
    glViewport(0, 0, m_viewportW, m_viewportH);

    setupQuad();

    // Create texture (dimensions set on first frame upload)
    glGenTextures(1, &m_textureId);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void Renderer::shutdown() {
    if (m_textureId) { glDeleteTextures(1, &m_textureId); m_textureId = 0; }
    if (m_vao)       { glDeleteVertexArrays(1, &m_vao);   m_vao = 0; }
    if (m_vbo)       { glDeleteBuffers(1, &m_vbo);        m_vbo = 0; }
    if (m_ebo)       { glDeleteBuffers(1, &m_ebo);        m_ebo = 0; }
}

void Renderer::uploadFrame(const cv::Mat& rgbFrame) {
    if (rgbFrame.empty()) return;

    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();

    glBindTexture(GL_TEXTURE_2D, m_textureId);

    if (rgbFrame.cols != m_texWidth || rgbFrame.rows != m_texHeight) {
        // Reallocate on resolution change
        m_texWidth  = rgbFrame.cols;
        m_texHeight = rgbFrame.rows;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                     m_texWidth, m_texHeight, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, rgbFrame.data);
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        m_texWidth, m_texHeight,
                        GL_RGB, GL_UNSIGNED_BYTE, rgbFrame.data);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    auto t1 = clock::now();
    m_lastUploadMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void Renderer::draw() {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();

    GLuint prog = m_shaderManager.activeProgram();
    if (!prog) return;

    glUseProgram(prog);

    // Bind camera texture to unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    m_shaderManager.setInt("uTexture", 0);

    // Pass texel size for effects that need it (edge detection, blur)
    if (m_texWidth > 0 && m_texHeight > 0) {
        m_shaderManager.setVec2("uTexelSize",
            1.0f / static_cast<float>(m_texWidth),
            1.0f / static_cast<float>(m_texHeight));
    }

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    auto t1 = clock::now();
    m_lastDrawMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

cv::Mat Renderer::getRenderedFrame() const {
    if (m_viewportW <= 0 || m_viewportH <= 0) return {};
    cv::Mat frame(m_viewportH, m_viewportW, CV_8UC3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    // Read from the back buffer before swap; origin is bottom-left in OpenGL.
    glReadPixels(0, 0, m_viewportW, m_viewportH, GL_RGB, GL_UNSIGNED_BYTE, frame.data);
    cv::flip(frame, frame, 0);   // flip to top-left origin
    return frame;
}

void Renderer::resize(int width, int height) {
    m_viewportW = width;
    m_viewportH = height;
    glViewport(0, 0, width, height);
}

std::string Renderer::lastError() const { return m_lastError; }

// ── private ──────────────────────────────────────────────────────────────────

void Renderer::setupQuad() {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kQuadIndices), kQuadIndices, GL_STATIC_DRAW);

    // position: location 0
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // texcoord: location 1
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}
