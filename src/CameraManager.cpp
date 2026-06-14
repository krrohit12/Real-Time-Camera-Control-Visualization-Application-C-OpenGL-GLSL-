#include "CameraManager.h"
#include <chrono>
#include <iostream>


// The four property-based controls we expose; Resolution and FPS are handled separately
static const std::vector<std::pair<std::string, int>> kProbeList = {
    {"Brightness", cv::CAP_PROP_BRIGHTNESS},
    {"Contrast",   cv::CAP_PROP_CONTRAST},
    {"Exposure",   cv::CAP_PROP_EXPOSURE},
    {"Gain",       cv::CAP_PROP_GAIN},
};

CameraManager::CameraManager() = default;

CameraManager::~CameraManager() {
    close();
}

bool CameraManager::open(int deviceIndex) {
    m_deviceIndex = deviceIndex;
    if (!m_cap.open(deviceIndex)) {
        std::unique_lock<std::mutex> lock(m_errorMutex);
        m_lastError = "Failed to open camera device " + std::to_string(deviceIndex);
        return false;
    }

    // Use platform-native backend for better control support
#ifdef __APPLE__
    // AVFoundation is the default on macOS; no explicit hint needed
#elif defined(_WIN32)
    m_cap.open(deviceIndex, cv::CAP_DSHOW);
#else
    m_cap.open(deviceIndex, cv::CAP_V4L2);
#endif

    if (!m_cap.isOpened()) {
        if (!m_cap.open(deviceIndex)) {
            std::unique_lock<std::mutex> lock(m_errorMutex);
            m_lastError = "Camera device not available: " + std::to_string(deviceIndex);
            return false;
        }
    }

    probeControls();
    m_isOpen.store(true);
    m_running.store(true);
    m_captureThread = std::thread(&CameraManager::captureLoop, this);
    return true;
}

void CameraManager::close() {
    m_running.store(false);
    m_frameQueue.stop();
    if (m_captureThread.joinable())
        m_captureThread.join();
    m_cap.release();
    m_isOpen.store(false);
}

bool CameraManager::isOpen() const {
    return m_isOpen.load();
}

bool CameraManager::setResolution(int width, int height) {
    m_cap.set(cv::CAP_PROP_FRAME_WIDTH,  width);
    m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    // Verify what the camera actually accepted
    int w = static_cast<int>(m_cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int h = static_cast<int>(m_cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    return (w == width && h == height);
}

bool CameraManager::setFPS(double fps) {
    m_cap.set(cv::CAP_PROP_FPS, fps);
    return true;
}

int CameraManager::getWidth() const {
    return static_cast<int>(m_cap.get(cv::CAP_PROP_FRAME_WIDTH));
}

int CameraManager::getHeight() const {
    return static_cast<int>(m_cap.get(cv::CAP_PROP_FRAME_HEIGHT));
}

double CameraManager::getFPS() const {
    return m_cap.get(cv::CAP_PROP_FPS);
}

std::vector<CameraControl>& CameraManager::controls() {
    return m_controls;
}

bool CameraManager::applyControl(int propId, double value) {
    // On Linux/Windows: disable auto-exposure before setting manual value
    if (propId == cv::CAP_PROP_EXPOSURE)
        m_cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25);

    bool ok = m_cap.set(propId, value);
    double accepted = m_cap.get(propId);
    for (auto& c : m_controls)
        if (c.propId == propId) { c.currentVal = accepted; break; }
    return ok;
}

std::string CameraManager::lastError() const {
    std::unique_lock<std::mutex> lock(m_errorMutex);
    return m_lastError;
}

void CameraManager::setAutoReconnect(bool enable, int deviceIndex) {
    m_autoReconnect = enable;
    m_deviceIndex   = deviceIndex;
}

double CameraManager::captureRateHz() const {
    return m_captureRate.load();
}

std::size_t CameraManager::totalFramesCaptured() const {
    return m_frameCount.load();
}

// ── private ──────────────────────────────────────────────────────────────────

void CameraManager::captureLoop() {
    using clock = std::chrono::steady_clock;
    auto fpsWindowStart = clock::now();
    int  fpsFrameCount  = 0;

    while (m_running.load()) {
        cv::Mat frame;
        auto t0 = clock::now();
        bool ok = m_cap.read(frame);
        m_lastCaptureMs.store(
            std::chrono::duration<double, std::milli>(clock::now() - t0).count());

        if (!ok || frame.empty()) {
            std::unique_lock<std::mutex> lock(m_errorMutex);
            m_lastError = "Frame read failed — camera may be disconnected";
            lock.unlock();

            if (m_autoReconnect) {
                attemptReconnect();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
            continue;
        }

        // AVFoundation on macOS can return non-contiguous frames (custom stride).
        // cv::cvtColor on a non-continuous mat causes a bus error on ARM64,
        // so we force a contiguous clone before the colour conversion.
        if (!frame.isContinuous())
            frame = frame.clone();

        // Convert to RGB for OpenGL; handle BGR and BGRA from AVFoundation
        cv::Mat rgb;
        if (frame.channels() == 4)
            cv::cvtColor(frame, rgb, cv::COLOR_BGRA2RGB);
        else
            cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

        double ts = static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                clock::now().time_since_epoch()).count());

        m_frameQueue.push({std::move(rgb), ts});
        ++m_frameCount;
        ++fpsFrameCount;

        auto now = clock::now();
        double elapsed = std::chrono::duration<double>(now - fpsWindowStart).count();
        if (elapsed >= 1.0) {
            m_captureRate.store(fpsFrameCount / elapsed);
            fpsFrameCount  = 0;
            fpsWindowStart = now;
        }
    }
}

void CameraManager::probeControls() {
    m_controls.clear();

    for (auto& [name, propId] : kProbeList) {
        double current  = m_cap.get(propId);
        bool   setOk    = m_cap.set(propId, current);
        double readback = m_cap.get(propId);

        CameraControl ctrl;
        ctrl.name       = name;
        ctrl.propId     = propId;
        ctrl.currentVal = readback;
        ctrl.defaultVal = readback;
        // A control is supported if the backend accepted the write and the
        // readback matches (within a small tolerance).
        ctrl.supported  = setOk && (std::abs(readback - current) < 1e-4);

        if (propId == cv::CAP_PROP_EXPOSURE) {
            ctrl.minVal = -13.0;
            ctrl.maxVal =   0.0;
        } else {
            ctrl.minVal = 0.0;
            ctrl.maxVal = (readback > 1.0) ? 255.0 : 1.0;
        }
        m_controls.push_back(ctrl);
    }
}

void CameraManager::attemptReconnect() {
    m_isOpen.store(false);
    m_cap.release();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (m_cap.open(m_deviceIndex)) {
        probeControls();
        m_isOpen.store(true);
        std::unique_lock<std::mutex> lock(m_errorMutex);
        m_lastError.clear();
    }
}
