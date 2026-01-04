#pragma once

#include <string>
#include <vector>

// Forward declaration
enum class WakeWordEventType;

// Socket writer class for sending wake word events and audio chunks to Python
// Uses RAII: resource acquired in constructor, released in destructor
// Throws std::runtime_error if initialization fails (fail-fast)
// Uses Unix Domain Sockets (AF_UNIX, SOCK_STREAM)
class SocketWriter {
public:
    // Constructor: Acquire resource, throw on failure (RAII)
    // Creates server socket, binds to path, listens for connections
    explicit SocketWriter(const std::string& socket_path);
    
    // Destructor: Release resource automatically (RAII)
    ~SocketWriter();
    
    // Delete copy constructor/assignment (non-copyable resource)
    SocketWriter(const SocketWriter&) = delete;
    SocketWriter& operator=(const SocketWriter&) = delete;
    
    // Move constructor (allows moving the resource)
    SocketWriter(SocketWriter&& other) noexcept;
    
    // Move assignment
    SocketWriter& operator=(SocketWriter&& other) noexcept;
    
    // Send event to socket (writes: "EVENT\n" + JSON + "\n")
    // Throws std::runtime_error on write failure
    void send_event(WakeWordEventType event_type);
    
    // Send audio chunk to socket (writes: "AUDIO\n" + size + binary data + "\n")
    // Throws std::runtime_error on write failure
    void send_audio_chunk(const std::vector<float>& audio_data, int sample_rate);

private:
    int server_fd_;        // Server socket file descriptor
    int client_fd_;        // Client connection file descriptor (-1 if not connected)
    std::string socket_path_; // Path to Unix Domain Socket
    
    // Convert event type to JSON string
    std::string event_to_json(WakeWordEventType event_type) const;
    
    // Ensure client connection is established (accept if needed)
    void ensure_connected();
    
    // Write data to socket (handles partial writes)
    void write_all(const void* data, size_t len);
};

