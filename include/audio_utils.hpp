#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>

// Audio format information
struct AudioFormat {
    int sample_rate = 16000;
    int channels = 1;
    bool is_stereo = false;
};

// Thread-safe generic queue for audio data
template<typename T>
class AudioQueue {
public:
    void push(const std::vector<T>& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(data);
        cond_var_.notify_one();
    }
    
    // Blocking pop
    std::vector<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !queue_.empty(); });
        std::vector<T> data = std::move(queue_.front());
        queue_.pop();
        return data;
    }
    
    // Try pop (non-blocking)
    bool try_pop(std::vector<T>& out_data) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        out_data = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // Wait for data with timeout
    bool wait_for_data(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cond_var_.wait_for(lock, timeout, [this] { return !queue_.empty(); });
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<std::vector<T>> empty;
        std::swap(queue_, empty);
    }

private:
    std::queue<std::vector<T>> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
};
