#pragma once

#include <opencv2/opencv.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <string>
#include <functional>

class SnapshotManager {
public:
    SnapshotManager();
    ~SnapshotManager();

    void start(const std::string& outputDir = ".");
    void stop();

    // Enqueue a frame for saving — non-blocking
    void saveAsync(const cv::Mat& rgbFrame);

    std::string lastSavedPath() const;
    std::string lastError() const;
    int         savedCount() const;

private:
    void workerLoop();
    std::string makeFilename() const;

    std::string m_outputDir{"."};

    struct Job { cv::Mat frame; std::string path; };

    std::queue<Job>         m_queue;
    std::mutex              m_mutex;
    std::condition_variable m_cv;
    std::thread             m_worker;
    std::atomic<bool>       m_running{false};

    mutable std::mutex      m_resultMutex;
    std::string             m_lastSaved;
    std::string             m_lastError;
    std::atomic<int>        m_savedCount{0};
};
