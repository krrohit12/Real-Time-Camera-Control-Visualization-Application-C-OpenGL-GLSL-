#pragma once

#include "CameraManager.h"
#include "ShaderManager.h"
#include "SnapshotManager.h"
#include "RecordingManager.h"
#include "MetricsManager.h"
#include <string>

// Desired resolution preset
struct ResolutionPreset {
    const char* label;
    int width;
    int height;
};

class ControlPanel {
public:
    ControlPanel();

    void draw(CameraManager&    camera,
              ShaderManager&    shaders,
              SnapshotManager&  snapshots,
              RecordingManager& recorder,
              MetricsManager&   metrics);

    // Shader-effect parameter accessors (set into shaders each frame)
    float brightness()  const { return m_brightness; }
    float contrast()    const { return m_contrast; }
    int   blurRadius()  const { return m_blurRadius; }
    float edgeStrength()const { return m_edgeStrength; }

    // Snapshot: Application polls and resets this each frame
    bool consumeSnapshotRequest() {
        bool v = m_snapshotRequested;
        m_snapshotRequested = false;
        return v;
    }

private:
    void drawCameraSection   (CameraManager& camera);
    void drawEffectsSection  (ShaderManager& shaders);
    void drawSnapshotSection (SnapshotManager& snapshots, const cv::Mat& lastFrame);
    void drawRecordingSection(RecordingManager& recorder, CameraManager& camera);
    void drawMetricsSection  (MetricsManager& metrics);

    // Effect parameters
    float m_brightness{0.0f};
    float m_contrast{1.0f};
    int   m_blurRadius{3};
    float m_edgeStrength{1.0f};

    bool        m_snapshotRequested{false};

    // Notification message
    std::string m_statusMsg;
    double      m_statusExpiry{0.0};

    // Recording state
    std::string m_recordingPath{"recording.mp4"};

    static const ResolutionPreset kResPresets[];
    int   m_resPresetIdx{0};
    float m_fpsValue{30.0f};
    bool  m_fpsAccepted{true};
    int   m_bufferSize{4};
    bool  m_bufferAccepted{true};
    int   m_queueMaxSize{4};
    int   m_renderFPSCap{0};

public:
    int targetRenderFPS() const { return m_renderFPSCap; }
};
