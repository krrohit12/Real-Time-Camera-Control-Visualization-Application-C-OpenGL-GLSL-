#pragma once

#include "CameraManager.h"
#include "Renderer.h"
#include "ControlPanel.h"
#include "SnapshotManager.h"
#include "RecordingManager.h"
#include "MetricsManager.h"

#include <GLFW/glfw3.h>
#include <chrono>
#include <thread>
#include <string>

class Application {
public:
    Application();
    ~Application();

    bool init(int windowWidth = 1280, int windowHeight = 720,
              const std::string& title = "Camera App",
              const std::string& shaderDir = "shaders");

    void run();
    void requestClose();

private:
    bool initGLFW(int w, int h, const std::string& title);
    bool initGLAD();
    bool initImGui();
    void shutdownImGui();
    void renderFrame();

    // GLFW callbacks (forwarded via user pointer)
    static void onFramebufferResize(GLFWwindow* w, int width, int height);

    GLFWwindow*      m_window{nullptr};
    CameraManager    m_camera;
    Renderer         m_renderer;
    ControlPanel     m_controlPanel;
    SnapshotManager  m_snapshot;
    RecordingManager m_recorder;
    MetricsManager   m_metrics;

    int    m_windowW{1280};
    int    m_windowH{720};

    // Latest RGB frame kept for snapshot / recording
    cv::Mat    m_currentFrame;
    std::mutex m_frameMutex;

    // Render FPS tracking
    using clock = std::chrono::steady_clock;
    clock::time_point m_lastFpsTick{clock::now()};
    int               m_fpsFrameCount{0};
    double            m_renderFPS{0.0};

    // Phase 3 — pipeline latency tracking
    double            m_captureTimestampMs{0.0}; // set when frame is dequeued
    double            m_renderTimestampMs{0.0};  // set just before glfwSwapBuffers
    double            m_pipelineLatencyMs{0.0};  // render - capture
};
