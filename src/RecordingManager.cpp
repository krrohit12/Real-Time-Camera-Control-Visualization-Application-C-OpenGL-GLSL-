#include "RecordingManager.h"
#include <iostream>

RecordingManager::RecordingManager() = default;

RecordingManager::~RecordingManager() {
    stopRecording();
}

bool RecordingManager::startRecording(const std::string& outputPath, double fps)
{
    if (m_recording.load()) {
        stopRecording();
    }

    m_outputPath   = outputPath;
    m_fps          = (fps > 0) ? fps : 30.0;
    m_writerReady  = false;   // VideoWriter opened lazily on first frame
    m_framesEncoded.store(0);

    {
        std::unique_lock<std::mutex> lock(m_resultMutex);
        m_lastError.clear();
    }

    m_recording.store(true);
    m_worker = std::thread(&RecordingManager::workerLoop, this);
    return true;
}

void RecordingManager::stopRecording() {
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_recording.store(false);
    }
    m_cv.notify_all();
    if (m_worker.joinable())
        m_worker.join();
    m_writer.release();
    m_writerReady = false;
}

bool RecordingManager::isRecording() const {
    return m_recording.load();
}

void RecordingManager::pushFrame(const cv::Mat& rgbFrame) {
    if (!m_recording.load() || rgbFrame.empty()) return;

    std::unique_lock<std::mutex> lock(m_mutex);

    // Open the VideoWriter on the very first frame so its dimensions match
    // exactly what we are recording — no mismatch, no resize, no crop.
    if (!m_writerReady) {
        if (!openWriter(rgbFrame.cols, rgbFrame.rows)) return;
        m_writerReady = true;
    }

    if (m_queue.size() >= kMaxQueueDepth)
        m_queue.pop();
    m_queue.push(rgbFrame.clone());
    lock.unlock();
    m_cv.notify_one();
}

std::string RecordingManager::lastSavedPath() const {
    return m_outputPath;
}

std::string RecordingManager::lastError() const {
    std::unique_lock<std::mutex> lock(m_resultMutex);
    return m_lastError;
}

int RecordingManager::framesEncoded() const {
    return m_framesEncoded.load();
}

// ── private ──────────────────────────────────────────────────────────────────

bool RecordingManager::openWriter(int width, int height) {
    // Try MP4 first, fall back to AVI
    int fourcc = cv::VideoWriter::fourcc('m','p','4','v');
    m_writer.open(m_outputPath, fourcc, m_fps, cv::Size(width, height), true);

    if (!m_writer.isOpened()) {
        std::string aviPath = m_outputPath.substr(0, m_outputPath.rfind('.')) + ".avi";
        m_outputPath = aviPath;
        fourcc = cv::VideoWriter::fourcc('M','J','P','G');
        m_writer.open(aviPath, fourcc, m_fps, cv::Size(width, height), true);
    }

    if (!m_writer.isOpened()) {
        std::unique_lock<std::mutex> lock(m_resultMutex);
        m_lastError = "Failed to open VideoWriter for: " + m_outputPath;
        m_recording.store(false);
        return false;
    }
    return true;
}

void RecordingManager::workerLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || !m_recording.load(); });

        if (!m_recording.load() && m_queue.empty())
            break;

        if (m_queue.empty()) continue;

        cv::Mat frame = std::move(m_queue.front());
        m_queue.pop();
        lock.unlock();

        // VideoWriter expects BGR
        cv::Mat bgr;
        cv::cvtColor(frame, bgr, cv::COLOR_RGB2BGR);
        m_writer.write(bgr);
        ++m_framesEncoded;
    }
}
