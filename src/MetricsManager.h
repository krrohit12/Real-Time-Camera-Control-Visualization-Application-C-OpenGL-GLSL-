#pragma once

#include <string>
#include <deque>
#include <mutex>
#include <chrono>
#include <fstream>

struct FrameMetrics {
    // Phase 2 — pipeline stage timings
    double captureRateHz{0.0};
    double captureMs{0.0};         // time to read one raw frame from camera
    double renderFPS{0.0};
    double uploadMs{0.0};          // GPU texture upload time
    double drawMs{0.0};            // glDraw* call time
    double queueDepth{0.0};
    double snapshotSaveMs{0.0};    // last imwrite() duration
    double recordingEncodeMs{0.0}; // last VideoWriter::write() duration
    std::size_t droppedFrames{0};
    std::size_t totalCaptured{0};

    // Phase 3 — software pipeline latency
    // captureTimestampMs : wall-clock ms when cap.read() returned in capture thread
    // renderTimestampMs  : wall-clock ms just before glfwSwapBuffers()
    // pipelineLatencyMs  : renderTimestampMs - captureTimestampMs
    //
    // This measures software pipeline latency only. It includes:
    //   queue wait time, texture upload, GLSL draw, ImGui render, swap delay.
    // It does NOT include camera sensor exposure or display panel response time.
    double captureTimestampMs{0.0};
    double renderTimestampMs{0.0};
    double pipelineLatencyMs{0.0};
};

class MetricsManager {
public:
    MetricsManager();
    ~MetricsManager();

    void startLogging(const std::string& logPath);
    void stopLogging();

    void update(const FrameMetrics& m);

    // Rolling averages (last N samples)
    double avgRenderFPS()          const;
    double avgUploadMs()           const;
    double avgDrawMs()             const;
    double avgPipelineLatencyMs()  const;

    // Raw latest (returns a copy — safe to call without holding the lock)
    FrameMetrics latest() const;

    // For plot history (ImGui)
    std::deque<float> renderFPSHistory()  const;
    std::deque<float> uploadMsHistory()   const;

private:
    static constexpr std::size_t kHistorySize = 128;

    mutable std::mutex      m_mutex;
    FrameMetrics            m_latest;
    std::deque<float>       m_fpsHistory;
    std::deque<float>       m_uploadHistory;
    std::deque<float>       m_latencyHistory;

    std::ofstream           m_logFile;
    bool                    m_logging{false};

    using clock = std::chrono::steady_clock;
    clock::time_point       m_lastRenderTick;
};
