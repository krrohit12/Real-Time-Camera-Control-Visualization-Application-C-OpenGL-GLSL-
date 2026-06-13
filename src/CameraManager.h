#pragma once

#include "ThreadSafeQueue.h"
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <functional>

struct CameraControl {
    std::string name;
    int         propId;
    double      minVal;
    double      maxVal;
    double      currentVal;
    double      defaultVal;
    bool        supported{false};   // confirmed writable on this camera
};

struct CameraFrame {
    cv::Mat     data;
    double      timestampMs;
};

class CameraManager {
public:
    CameraManager();
    ~CameraManager();

    bool open(int deviceIndex = 0);
    void close();
    bool isOpen() const;

    // Resolution / FPS
    bool setResolution(int width, int height);
    bool setFPS(double fps);
    int  getWidth()  const;
    int  getHeight() const;
    double getFPS() const;

    // Dynamic camera controls
    std::vector<CameraControl>& controls();
    bool applyControl(int propId, double value);

    // Frame queue shared with renderer
    ThreadSafeQueue<CameraFrame>& frameQueue() { return m_frameQueue; }

    // Error state
    std::string lastError() const;

    // Reconnect on disconnect
    void setAutoReconnect(bool enable, int deviceIndex = 0);

    // Metrics
    double captureRateHz() const;
    std::size_t totalFramesCaptured() const;

private:
    void captureLoop();
    void probeControls();
    void attemptReconnect();

    cv::VideoCapture            m_cap;
    std::thread                 m_captureThread;
    std::atomic<bool>           m_running{false};
    std::atomic<bool>           m_isOpen{false};
    ThreadSafeQueue<CameraFrame> m_frameQueue{4};

    std::vector<CameraControl>  m_controls;
    mutable std::mutex          m_errorMutex;
    std::string                 m_lastError;

    int    m_deviceIndex{0};
    bool   m_autoReconnect{true};

    std::atomic<std::size_t>    m_frameCount{0};
    std::atomic<double>         m_captureRate{0.0};
};
