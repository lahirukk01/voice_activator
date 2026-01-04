#pragma once

#include <string>

// Forward declaration
enum class WakeWordEventType;

// FIFO writer class for sending wake word events to Python
// Uses RAII: resource acquired in constructor, released in destructor
// Throws std::runtime_error if initialization fails (fail-fast)
class FifoWriter {
public:
    // Constructor: Acquire resource, throw on failure (RAII)
    explicit FifoWriter(const std::string& fifo_path);
    
    // Destructor: Release resource automatically (RAII)
    ~FifoWriter();
    
    // Delete copy constructor/assignment (non-copyable resource)
    FifoWriter(const FifoWriter&) = delete;
    FifoWriter& operator=(const FifoWriter&) = delete;
    
    // Move constructor (allows moving the resource)
    FifoWriter(FifoWriter&& other) noexcept;
    
    // Move assignment
    FifoWriter& operator=(FifoWriter&& other) noexcept;
    
    // Send event to FIFO (writes JSON: {"type":"START"} or {"type":"STOP"})
    // Throws std::runtime_error on write failure
    void send_event(WakeWordEventType event_type);

private:
    int fifo_fd_;           // File descriptor for FIFO
    std::string fifo_path_; // Path to FIFO
    
    // Convert event type to JSON string
    std::string event_to_json(WakeWordEventType event_type) const;
};
