#include "socket_writer.hpp"
#include "wake_word_event.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <arpa/inet.h>

// Constructor: Acquire resource, throw on failure (RAII)
SocketWriter::SocketWriter(const std::string& socket_path) 
    : server_fd_(-1)
    , client_fd_(-1)
    , socket_path_(socket_path)
{
    // Remove existing socket file if it exists
    unlink(socket_path.c_str());
    
    // Create Unix Domain Socket
    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error(
            "Failed to create socket: " + std::string(strerror(errno))
        );
    }
    
    // Bind socket to path
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
    
    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(server_fd_);
        server_fd_ = -1;
        throw std::runtime_error(
            "Failed to bind socket to " + socket_path + ": " + strerror(errno)
        );
    }
    
    // Listen for connections (backlog of 1)
    if (listen(server_fd_, 1) < 0) {
        ::close(server_fd_);
        server_fd_ = -1;
        throw std::runtime_error(
            "Failed to listen on socket: " + std::string(strerror(errno))
        );
    }
    
    // Set server socket to non-blocking for accept
    int flags = fcntl(server_fd_, F_GETFL, 0);
    fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);
    
    std::cout << "Socket server listening on: " << socket_path << std::endl;
    std::cout << "Waiting for Python client to connect..." << std::endl;
}

// Destructor: Release resource automatically (RAII)
SocketWriter::~SocketWriter() {
    if (client_fd_ >= 0) {
        ::close(client_fd_);
    }
    if (server_fd_ >= 0) {
        ::close(server_fd_);
        unlink(socket_path_.c_str());
        std::cout << "Socket closed: " << socket_path_ << std::endl;
    }
}

// Move constructor
SocketWriter::SocketWriter(SocketWriter&& other) noexcept
    : server_fd_(other.server_fd_)
    , client_fd_(other.client_fd_)
    , socket_path_(std::move(other.socket_path_))
{
    other.server_fd_ = -1;
    other.client_fd_ = -1;
}

// Move assignment
SocketWriter& SocketWriter::operator=(SocketWriter&& other) noexcept {
    if (this != &other) {
        // Close current resources
        if (client_fd_ >= 0) {
            ::close(client_fd_);
        }
        if (server_fd_ >= 0) {
            ::close(server_fd_);
            unlink(socket_path_.c_str());
        }
        
        // Move from other
        server_fd_ = other.server_fd_;
        client_fd_ = other.client_fd_;
        socket_path_ = std::move(other.socket_path_);
        other.server_fd_ = -1;
        other.client_fd_ = -1;
    }
    return *this;
}

// Ensure client connection is established
void SocketWriter::ensure_connected() {
    // If already connected, check if connection is still alive
    if (client_fd_ >= 0) {
        // Quick check: try to get socket error
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(client_fd_, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
            return; // Connection is good
        }
        // Connection is broken, close it
        ::close(client_fd_);
        client_fd_ = -1;
    }
    
    // Try to accept a connection (non-blocking)
    struct sockaddr_un client_addr;
    socklen_t client_len = sizeof(client_addr);
    client_fd_ = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
    
    if (client_fd_ >= 0) {
        std::cout << "Client connected to socket" << std::endl;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        // Real error (not just "no connection yet")
        std::cerr << "Warning: Failed to accept connection: " << strerror(errno) << std::endl;
    }
    // If EAGAIN/EWOULDBLOCK, no client connected yet - that's OK, we'll retry
}

// Write all data (handles partial writes)
void SocketWriter::write_all(const void* data, size_t len) {
    const char* buf = static_cast<const char*>(data);
    size_t total_written = 0;
    
    while (total_written < len) {
        ssize_t written = write(client_fd_, buf + total_written, len - total_written);
        if (written < 0) {
            if (errno == EPIPE || errno == ECONNRESET) {
                // Client disconnected
                ::close(client_fd_);
                client_fd_ = -1;
                throw std::runtime_error("Client disconnected");
            } else {
                throw std::runtime_error(
                    "Failed to write to socket: " + std::string(strerror(errno))
                );
            }
        }
        total_written += written;
    }
}

// Send event to socket
void SocketWriter::send_event(WakeWordEventType event_type) {
    ensure_connected();
    
    if (client_fd_ < 0) {
        // No client connected yet - skip event (only log once)
        static bool logged_no_client = false;
        if (!logged_no_client) {
            std::cerr << "Warning: No client connected, events will be dropped until client connects" << std::endl;
            logged_no_client = true;
        }
        return;
    }
    
    std::string json = event_to_json(event_type);
    
    try {
        // Write: "EVENT\n" + JSON + "\n"
        const char* event_prefix = "EVENT\n";
        write_all(event_prefix, strlen(event_prefix));
        write_all(json.c_str(), json.size());
        const char* newline = "\n";
        write_all(newline, 1);
    } catch (const std::exception& e) {
        // Connection lost, will retry on next send
        std::cerr << "Warning: Failed to send event: " << e.what() << std::endl;
        if (client_fd_ >= 0) {
            ::close(client_fd_);
            client_fd_ = -1;
        }
    }
}

// Send audio chunk to socket
void SocketWriter::send_audio_chunk(const std::vector<float>& audio_data, int sample_rate) {
    ensure_connected();
    
    if (client_fd_ < 0) {
        // No client connected - skip audio chunk
        return;
    }
    
    if (audio_data.empty()) {
        return; // Nothing to send
    }
    
    try {
        // Write: "AUDIO\n" + size (4 bytes, network byte order) + binary data + "\n"
        const char* audio_prefix = "AUDIO\n";
        write_all(audio_prefix, strlen(audio_prefix));
        
        // Write size (uint32_t in network byte order)
        uint32_t size = static_cast<uint32_t>(audio_data.size() * sizeof(float));
        uint32_t size_net = htonl(size);
        write_all(&size_net, sizeof(size_net));
        
        // Write binary float data
        write_all(audio_data.data(), size);
        
        // Write sample rate (uint32_t in network byte order)
        uint32_t rate_net = htonl(static_cast<uint32_t>(sample_rate));
        write_all(&rate_net, sizeof(rate_net));
        
        // Write newline terminator
        const char* newline = "\n";
        write_all(newline, 1);
    } catch (const std::exception& e) {
        // Connection lost, will retry on next send
        std::cerr << "Warning: Failed to send audio chunk: " << e.what() << std::endl;
        if (client_fd_ >= 0) {
            ::close(client_fd_);
            client_fd_ = -1;
        }
    }
}

std::string SocketWriter::event_to_json(WakeWordEventType event_type) const {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"" 
        << (event_type == WakeWordEventType::START ? "START" : "STOP") 
        << "\""
        << "}";
    return oss.str();
}

