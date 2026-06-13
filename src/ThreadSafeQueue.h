#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <cstddef>

template <typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(std::size_t maxSize = 4)
        : m_maxSize(maxSize) {}

    // Push an item; drops oldest if full (keeps latency low)
    void push(T item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queue.size() >= m_maxSize) {
            m_queue.pop();
            ++m_droppedCount;
        }
        m_queue.push(std::move(item));
        lock.unlock();
        m_cv.notify_one();
    }

    // Block until an item is available or stop is signalled
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || m_stopped; });
        if (m_queue.empty())
            return std::nullopt;
        T item = std::move(m_queue.front());
        m_queue.pop();
        return item;
    }

    // Non-blocking try-pop
    std::optional<T> tryPop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queue.empty())
            return std::nullopt;
        T item = std::move(m_queue.front());
        m_queue.pop();
        return item;
    }

    void stop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_stopped = true;
        lock.unlock();
        m_cv.notify_all();
    }

    void reset() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_stopped = false;
        while (!m_queue.empty()) m_queue.pop();
    }

    std::size_t size() const {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    std::size_t droppedCount() const {
        return m_droppedCount.load();
    }

    void resetDropCount() {
        m_droppedCount.store(0);
    }

private:
    mutable std::mutex          m_mutex;
    std::condition_variable     m_cv;
    std::queue<T>               m_queue;
    std::size_t                 m_maxSize;
    bool                        m_stopped = false;
    std::atomic<std::size_t>    m_droppedCount{0};
};
