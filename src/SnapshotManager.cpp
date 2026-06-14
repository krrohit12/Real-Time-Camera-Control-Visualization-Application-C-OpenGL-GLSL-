#include "SnapshotManager.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

SnapshotManager::SnapshotManager() = default;

SnapshotManager::~SnapshotManager() {
    stop();
}

void SnapshotManager::start(const std::string& outputDir) {
    m_outputDir = outputDir;
    m_running.store(true);
    m_worker = std::thread(&SnapshotManager::workerLoop, this);
}

void SnapshotManager::stop() {
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_running.store(false);
    }
    m_cv.notify_all();
    if (m_worker.joinable())
        m_worker.join();
}

void SnapshotManager::saveAsync(const cv::Mat& rgbFrame) {
    if (rgbFrame.empty()) return;
    cv::Mat copy = rgbFrame.clone();
    std::string path = makeFilename();
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.push({std::move(copy), path});
    }
    m_cv.notify_one();
}

std::string SnapshotManager::lastSavedPath() const {
    std::unique_lock<std::mutex> lock(m_resultMutex);
    return m_lastSaved;
}

std::string SnapshotManager::lastError() const {
    std::unique_lock<std::mutex> lock(m_resultMutex);
    return m_lastError;
}

int SnapshotManager::savedCount() const {
    return m_savedCount.load();
}

// ── private ──────────────────────────────────────────────────────────────────

void SnapshotManager::workerLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || !m_running.load(); });

        if (!m_running.load() && m_queue.empty())
            break;

        Job job = std::move(m_queue.front());
        m_queue.pop();
        lock.unlock();

        // Convert RGB back to BGR for OpenCV imwrite
        cv::Mat bgr;
        cv::cvtColor(job.frame, bgr, cv::COLOR_RGB2BGR);

        using clock = std::chrono::steady_clock;
        auto t0    = clock::now();
        bool ok    = cv::imwrite(job.path, bgr);
        m_lastSaveMs.store(
            std::chrono::duration<double, std::milli>(clock::now() - t0).count());
        if (ok) {
            ++m_savedCount;
            std::unique_lock<std::mutex> r(m_resultMutex);
            m_lastSaved = job.path;
        } else {
            std::unique_lock<std::mutex> r(m_resultMutex);
            m_lastError = "Failed to write: " + job.path;
            std::cerr << "[SnapshotManager] " << m_lastError << "\n";
        }
    }
}

std::string SnapshotManager::makeFilename() const {
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_info{};
#ifdef _WIN32
    localtime_s(&tm_info, &time);
#else
    localtime_r(&time, &tm_info);
#endif
    std::ostringstream ss;
    ss << m_outputDir << "/snapshot_"
       << std::put_time(&tm_info, "%Y%m%d_%H%M%S")
       << ".png";
    return ss.str();
}
