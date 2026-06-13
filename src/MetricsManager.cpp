#include "MetricsManager.h"
#include <numeric>
#include <iomanip>
#include <iostream>

MetricsManager::MetricsManager() {
    m_lastRenderTick = clock::now();
    m_renderFrameStart = clock::now();
}

MetricsManager::~MetricsManager() {
    stopLogging();
}

void MetricsManager::startLogging(const std::string& logPath) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_logFile.open(logPath, std::ios::out | std::ios::trunc);
    if (m_logFile.is_open()) {
        m_logFile << "timestamp_ms,render_fps,capture_hz,upload_ms,draw_ms,"
                     "queue_depth,dropped_frames,total_captured\n";
        m_logging = true;
    }
}

void MetricsManager::stopLogging() {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_logFile.is_open()) m_logFile.close();
    m_logging = false;
}

void MetricsManager::update(const FrameMetrics& m) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_latest = m;

    m_fpsHistory.push_back(static_cast<float>(m.renderFPS));
    if (m_fpsHistory.size() > kHistorySize) m_fpsHistory.pop_front();

    m_uploadHistory.push_back(static_cast<float>(m.uploadMs));
    if (m_uploadHistory.size() > kHistorySize) m_uploadHistory.pop_front();

    if (m_logging && m_logFile.is_open()) {
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now().time_since_epoch()).count();
        m_logFile << nowMs << ","
                  << std::fixed << std::setprecision(2)
                  << m.renderFPS     << ","
                  << m.captureRateHz << ","
                  << m.uploadMs      << ","
                  << m.drawMs        << ","
                  << m.queueDepth    << ","
                  << m.droppedFrames << ","
                  << m.totalCaptured << "\n";
    }
}

double MetricsManager::avgRenderFPS() const {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_fpsHistory.empty()) return 0.0;
    double sum = std::accumulate(m_fpsHistory.begin(), m_fpsHistory.end(), 0.0f);
    return sum / m_fpsHistory.size();
}

double MetricsManager::avgUploadMs() const {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_uploadHistory.empty()) return 0.0;
    double sum = std::accumulate(m_uploadHistory.begin(), m_uploadHistory.end(), 0.0f);
    return sum / m_uploadHistory.size();
}

double MetricsManager::avgDrawMs() const {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_latest.drawMs;
}

const FrameMetrics& MetricsManager::latest() const {
    // Caller must not hold lock; returns a copy-safe snapshot via ref
    // (protected by update's lock — safe for single consumer)
    return m_latest;
}

std::deque<float> MetricsManager::renderFPSHistory() const {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_fpsHistory;
}

std::deque<float> MetricsManager::uploadMsHistory() const {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_uploadHistory;
}
