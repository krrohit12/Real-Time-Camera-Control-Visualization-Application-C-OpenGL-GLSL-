#pragma once

#include <opencv2/opencv.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <string>

class RecordingManager {
public:
    RecordingManager();
    ~RecordingManager();

    // Width/height are omitted — the VideoWriter initializes itself from the
    // first frame pushed, so dimensions always match what is actually recorded.
    bool startRecording(const std::string& outputPath, double fps);
    void stopRecording();
    bool isRecording() const;

    // Enqueue a frame — non-blocking, drops if queue is full
    void pushFrame(const cv::Mat& rgbFrame);

    std::string lastSavedPath() const;
    std::string lastError() const;
    int         framesEncoded() const;

private:
    void workerLoop();
    bool openWriter(int width, int height);  // called lazily on first frame

    cv::VideoWriter         m_writer;
    std::string             m_outputPath;
    double                  m_fps{30.0};
    bool                    m_writerReady{false};  // guarded by m_mutex

    std::queue<cv::Mat>     m_queue;
    std::mutex              m_mutex;
    std::condition_variable m_cv;
    std::thread             m_worker;
    std::atomic<bool>       m_recording{false};

    static constexpr std::size_t kMaxQueueDepth = 30;

    mutable std::mutex      m_resultMutex;
    std::string             m_lastError;
    std::atomic<int>        m_framesEncoded{0};
};
