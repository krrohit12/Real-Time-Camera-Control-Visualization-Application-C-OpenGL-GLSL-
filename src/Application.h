#pragma once

#include "CameraManager.h"
#include "Renderer.h"
#include "ControlPanel.h"
#include "SnapshotManager.h"
#include "RecordingManager.h"

#include <GLFW/glfw3.h>
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

    int    m_windowW{1280};
    int    m_windowH{720};

    // Latest RGB frame kept for snapshot / recording
    cv::Mat    m_currentFrame;
    std::mutex m_frameMutex;
};
