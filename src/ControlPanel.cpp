#include "ControlPanel.h"
#include <imgui.h>
#include <chrono>
#include <cstring>

const ResolutionPreset ControlPanel::kResPresets[] = {
    {"320x240",   320,  240},
    {"640x480",   640,  480},
    {"1280x720", 1280,  720},
    {"1920x1080",1920, 1080},
};

static double nowSec() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

ControlPanel::ControlPanel() = default;

void ControlPanel::draw(CameraManager&    camera,
                        ShaderManager&    shaders,
                        SnapshotManager&  snapshots,
                        RecordingManager& recorder,
                        MetricsManager&   metrics)
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Once);
    ImGui::Begin("Camera Controls", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

    drawCameraSection(camera);
    ImGui::Separator();
    drawEffectsSection(shaders);
    ImGui::Separator();
    drawSnapshotSection(snapshots, cv::Mat{});
    ImGui::Separator();
    drawRecordingSection(recorder, camera);
    ImGui::Separator();
    drawMetricsSection(metrics);

    // Status notification
    if (!m_statusMsg.empty() && nowSec() < m_statusExpiry) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", m_statusMsg.c_str());
    }

    ImGui::End();
}

// ── private sections ─────────────────────────────────────────────────────────

void ControlPanel::drawCameraSection(CameraManager& camera) {
    if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Text("Status: %s", camera.isOpen() ? "Open" : "Closed");
    if (!camera.lastError().empty())
        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "Error: %s",
                           camera.lastError().c_str());

    if (!camera.isOpen()) return;

    // ── Brightness, Contrast, Exposure, Gain ─────────────────────────────────
    for (auto& ctrl : camera.controls()) {
        float val  = static_cast<float>(ctrl.currentVal);
        float minV = static_cast<float>(ctrl.minVal);
        float maxV = static_cast<float>(ctrl.maxVal);

        if (!ctrl.supported) {
            ImGui::BeginDisabled();
            ImGui::SliderFloat(ctrl.name.c_str(), &val, minV, maxV);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("N/A");
        } else {
            if (ImGui::SliderFloat(ctrl.name.c_str(), &val, minV, maxV)) {
                ctrl.currentVal = static_cast<double>(val);
                camera.applyControl(ctrl.propId, ctrl.currentVal);
            }
        }
    }

    // ── Resolution ────────────────────────────────────────────────────────────
    constexpr int kNumPresets = sizeof(kResPresets) / sizeof(kResPresets[0]);
    if (ImGui::BeginCombo("Resolution", kResPresets[m_resPresetIdx].label)) {
        for (int i = 0; i < kNumPresets; ++i) {
            bool selected = (m_resPresetIdx == i);
            if (ImGui::Selectable(kResPresets[i].label, selected)) {
                m_resPresetIdx = i;
                camera.setResolution(kResPresets[i].width, kResPresets[i].height);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // ── FPS ───────────────────────────────────────────────────────────────────
    if (ImGui::SliderFloat("FPS", &m_fpsValue, 5.0f, 60.0f, "%.0f")) {
        camera.setFPS(static_cast<double>(m_fpsValue));
    }
}

void ControlPanel::drawEffectsSection(ShaderManager& shaders) {
    if (!ImGui::CollapsingHeader("Shader Effects", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    auto& effects = shaders.effects();
    if (effects.empty()) {
        ImGui::TextDisabled("No shaders loaded");
        return;
    }

    int active = shaders.activeEffectIndex();
    for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
        if (ImGui::RadioButton(effects[i].name.c_str(), &active, i))
            shaders.setActiveEffect(i);
    }

    // Show relevant parameter sliders based on active effect
    const std::string& name = effects[active].name;

    if (name == "Brightness/Contrast") {
        ImGui::SliderFloat("Brightness", &m_brightness, -1.0f, 1.0f);
        ImGui::SliderFloat("Contrast",   &m_contrast,   0.0f, 3.0f);
    } else if (name == "Blur") {
        ImGui::SliderInt("Blur Radius", &m_blurRadius, 1, 10);
    } else if (name == "Edge Detection") {
        ImGui::SliderFloat("Edge Strength", &m_edgeStrength, 0.1f, 5.0f);
    }
}

void ControlPanel::drawSnapshotSection(SnapshotManager& snapshots, const cv::Mat&) {
    if (!ImGui::CollapsingHeader("Snapshot"))
        return;

    ImGui::Text("Saved: %d", snapshots.savedCount());

    std::string lastSaved = snapshots.lastSavedPath();
    if (!lastSaved.empty())
        ImGui::TextDisabled("%s", lastSaved.c_str());

    if (!snapshots.lastError().empty())
        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "%s",
                           snapshots.lastError().c_str());

    if (ImGui::Button("Take Snapshot")) {
        m_snapshotRequested = true;
        m_statusMsg         = "Snapshot queued";
        m_statusExpiry      = nowSec() + 2.0;
    }
}

void ControlPanel::drawRecordingSection(RecordingManager& recorder,
                                        CameraManager& camera)
{
    if (!ImGui::CollapsingHeader("Recording"))
        return;

    bool rec = recorder.isRecording();
    if (rec) {
        ImGui::TextColored(ImVec4(1,0.2f,0.2f,1), "● REC  %d frames",
                           recorder.framesEncoded());
        if (ImGui::Button("Stop Recording")) {
            recorder.stopRecording();
            m_statusMsg    = "Saved: " + recorder.lastSavedPath();
            m_statusExpiry = nowSec() + 4.0;
        }
    } else {
        static char pathBuf[256] = "recording.mp4";
        ImGui::InputText("Output file", pathBuf, sizeof(pathBuf));
        if (ImGui::Button("Start Recording")) {
            double fps = camera.getFPS();
            if (fps <= 0) fps = 30.0;
            recorder.startRecording(pathBuf, fps);
        }
    }

    if (!recorder.lastError().empty())
        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "%s",
                           recorder.lastError().c_str());
}

void ControlPanel::drawMetricsSection(MetricsManager& metrics) {
    if (!ImGui::CollapsingHeader("Metrics", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    const FrameMetrics m = metrics.latest();

    ImGui::Text("Render FPS:      %.1f  (avg %.1f)",
                m.renderFPS, metrics.avgRenderFPS());
    ImGui::Text("Capture Rate:    %.1f Hz", m.captureRateHz);
    ImGui::Text("Capture Time:    %.2f ms", m.captureMs);
    ImGui::Text("Upload Time:     %.2f ms  (avg %.2f)",
                m.uploadMs, metrics.avgUploadMs());
    ImGui::Text("Render Time:     %.2f ms  (avg %.2f)",
                m.drawMs, metrics.avgDrawMs());
    ImGui::Text("Queue Depth:     %.0f", m.queueDepth);
    ImGui::Text("Dropped Frames:  %zu", m.droppedFrames);
    ImGui::Text("Total Captured:  %zu", m.totalCaptured);

    if (m.snapshotSaveMs > 0.0)
        ImGui::Text("Snapshot Save:   %.1f ms", m.snapshotSaveMs);
    if (m.recordingEncodeMs > 0.0)
        ImGui::Text("Encode Time:     %.2f ms", m.recordingEncodeMs);

    ImGui::Spacing();

    static char logPathBuf[256] = "metrics.csv";
    static bool logging = false;

    if (!logging) {
        ImGui::InputText("Log file", logPathBuf, sizeof(logPathBuf));
        if (ImGui::Button("Start Logging")) {
            metrics.startLogging(logPathBuf);
            logging = true;
        }
    } else {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
                           "Logging -> %s", logPathBuf);
        if (ImGui::Button("Stop Logging")) {
            metrics.stopLogging();
            logging = false;
        }
    }
}
