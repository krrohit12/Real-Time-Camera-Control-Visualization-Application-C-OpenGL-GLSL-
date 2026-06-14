#include "Application.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>

Application::Application() = default;

Application::~Application() {
    m_camera.close();
    m_snapshot.stop();
    m_recorder.stopRecording();
    shutdownImGui();
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool Application::init(int windowWidth, int windowHeight,
                       const std::string& title,
                       const std::string& shaderDir)
{
    m_windowW = windowWidth;
    m_windowH = windowHeight;

    if (!initGLFW(windowWidth, windowHeight, title)) return false;
    if (!initGLAD())  return false;
    if (!initImGui()) return false;

    // On Retina / HiDPI displays the physical framebuffer is larger than the
    // logical window size. Query the real pixel dimensions so that
    // glReadPixels captures the full frame rather than a sub-region.
    int fbW = windowWidth, fbH = windowHeight;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);

    if (!m_renderer.init(fbW, fbH)) {
        std::cerr << "Renderer init failed: " << m_renderer.lastError() << "\n";
        return false;
    }

    if (!m_renderer.shaderManager().init(shaderDir)) {
        std::cerr << "ShaderManager: some shaders failed. "
                  << m_renderer.shaderManager().lastError() << "\n";
        if (!m_renderer.shaderManager().hasEffects()) return false;
    }

    m_snapshot.start(".");
    m_lastFpsTick = clock::now();

    if (!m_camera.open(0)) {
        std::cerr << "Warning: " << m_camera.lastError()
                  << " — continuing without camera\n";
    }

    return true;
}

void Application::run() {
    while (!glfwWindowShouldClose(m_window)) {
        auto frameStart = clock::now();
        glfwPollEvents();
        renderFrame();

        int cap = m_controlPanel.targetRenderFPS();
        if (cap > 0) {
            auto target  = std::chrono::duration<double>(1.0 / cap);
            auto elapsed = clock::now() - frameStart;
            if (elapsed < target)
                std::this_thread::sleep_for(target - elapsed);
        }
    }
}

void Application::requestClose() {
    if (m_window) glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

// ── private ──────────────────────────────────────────────────────────────────

bool Application::initGLFW(int w, int h, const std::string& title) {
    if (!glfwInit()) {
        std::cerr << "GLFW init failed\n";
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    m_window = glfwCreateWindow(w, h, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        std::cerr << "GLFW window creation failed\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // vsync

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, onFramebufferResize);
    return true;
}

bool Application::initGLAD() {
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "GLAD load failed\n";
        return false;
    }
    return true;
}

bool Application::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    if (!ImGui_ImplGlfw_InitForOpenGL(m_window, true)) return false;
    if (!ImGui_ImplOpenGL3_Init("#version 410 core")) return false;
    return true;
}

void Application::shutdownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Application::renderFrame() {
    // ── 1. Pull latest camera frame ──────────────────────────────────────────
    bool newCameraFrame = false;
    auto optFrame = m_camera.frameQueue().tryPop();
    if (optFrame.has_value()) {
        m_captureTimestampMs = optFrame->timestampMs; // Phase 3: when frame left camera
        std::unique_lock<std::mutex> lock(m_frameMutex);
        m_currentFrame = std::move(optFrame->data);
        lock.unlock();
        m_renderer.uploadFrame(m_currentFrame);
        newCameraFrame = true;
    }

    // ── 2. Set per-frame shader uniforms ────────────────────────────────────
    auto& sm = m_renderer.shaderManager();
    sm.setFloat("uBrightness",   m_controlPanel.brightness());
    sm.setFloat("uContrast",     m_controlPanel.contrast());
    sm.setFloat("uEdgeStrength", m_controlPanel.edgeStrength());
    sm.setInt  ("uBlurRadius",   m_controlPanel.blurRadius());

    // ── 3. Render camera frame with active shader applied ────────────────────
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    m_renderer.draw();

    // ── 4. Capture shader-processed frame for snapshot / recording ───────────
    // Read back after draw() but before ImGui so the UI is not baked in.
    bool snapshotWanted = m_controlPanel.consumeSnapshotRequest();
    bool needsReadback  = snapshotWanted || (m_recorder.isRecording() && newCameraFrame);

    if (needsReadback && !m_currentFrame.empty()) {
        cv::Mat rendered = m_renderer.getRenderedFrame();
        if (!rendered.empty()) {
            if (snapshotWanted)
                m_snapshot.saveAsync(rendered);

            if (m_recorder.isRecording() && newCameraFrame)
                m_recorder.pushFrame(rendered);
        }
    }

    // ── 5. Collect metrics ───────────────────────────────────────────────────
    ++m_fpsFrameCount;
    auto now = clock::now();
    double elapsed = std::chrono::duration<double>(now - m_lastFpsTick).count();
    if (elapsed >= 1.0) {
        m_renderFPS      = m_fpsFrameCount / elapsed;
        m_fpsFrameCount  = 0;
        m_lastFpsTick    = now;
    }

    {
        FrameMetrics fm;
        fm.renderFPS         = m_renderFPS;
        fm.captureRateHz     = m_camera.captureRateHz();
        fm.captureMs         = m_camera.lastCaptureMs();
        fm.uploadMs          = m_renderer.lastUploadMs();
        fm.drawMs            = m_renderer.lastDrawMs();
        fm.queueDepth        = static_cast<double>(m_camera.frameQueue().size());
        fm.droppedFrames     = m_camera.frameQueue().droppedCount();
        fm.totalCaptured     = m_camera.totalFramesCaptured();
        fm.snapshotSaveMs       = m_snapshot.lastSaveMs();
        fm.recordingEncodeMs    = m_recorder.lastEncodeMs();
        fm.captureTimestampMs   = m_captureTimestampMs;
        fm.renderTimestampMs    = m_renderTimestampMs;
        fm.pipelineLatencyMs    = m_pipelineLatencyMs;
        m_metrics.update(fm);
    }

    // ── 6. ImGui ─────────────────────────────────────────────────────────────
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    m_controlPanel.draw(m_camera, sm, m_snapshot, m_recorder, m_metrics);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // ── 7. Swap buffers ──────────────────────────────────────────────────────
    // Phase 3: capture render timestamp just before swap (frame about to be displayed)
    m_renderTimestampMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now().time_since_epoch()).count());

    if (m_captureTimestampMs > 0.0)
        m_pipelineLatencyMs = m_renderTimestampMs - m_captureTimestampMs;

    glfwSwapBuffers(m_window);
}

void Application::onFramebufferResize(GLFWwindow* w, int width, int height) {
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(w));
    app->m_renderer.resize(width, height);
    app->m_windowW = width;
    app->m_windowH = height;
}
