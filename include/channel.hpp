#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

// Go-like channel implementation for thread-safe communication
template <typename T>
class Channel {
private:
    std::queue<T> queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool closed_;

public:
    Channel() : closed_(false) {}
    
    // Go's "ch <- val" - Send a value to the channel
    void send(T val) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (closed_) {
            return;  // Channel is closed, ignore sends
        }
        queue_.push(val);
        cv_.notify_one();  // Wake up the receiver
    }

    // Go's "val := <- ch" - Receive a value from the channel (blocks until available)
    T receive() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return !queue_.empty() || closed_; });
        
        if (queue_.empty() && closed_) {
            // Return default value if channel is closed and empty
            return T{};
        }
        
        T val = queue_.front();
        queue_.pop();
        return val;
    }
    
    // Receive with timeout (returns false if timeout, true if value received)
    // timeout_ms: timeout in milliseconds
    template<typename Rep, typename Period>
    bool receive_timeout(T& val, const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mtx_);
        bool result = cv_.wait_for(lock, timeout, [this] { return !queue_.empty() || closed_; });
        
        if (!result || (queue_.empty() && closed_)) {
            return false;  // Timeout or channel closed
        }
        
        val = queue_.front();
        queue_.pop();
        return true;
    }
    
    // Try to receive without blocking (returns false if no value available)
    bool try_receive(T& val) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty()) {
            return false;
        }
        val = queue_.front();
        queue_.pop();
        return true;
    }
    
    // Close the channel (no more sends allowed)
    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        closed_ = true;
        cv_.notify_all();  // Wake up all waiting receivers
    }
    
    // Check if channel is closed
    bool is_closed() {
        std::lock_guard<std::mutex> lock(mtx_);
        return closed_;
    }
};

