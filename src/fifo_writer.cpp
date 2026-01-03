#include "fifo_writer.hpp"
#include "wake_word_event.hpp"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <iostream>

// Constructor: Acquire resource, throw on failure (RAII)
FifoWriter::FifoWriter(const std::string& fifo_path) 
    : fifo_fd_(-1)
    , fifo_path_(fifo_path)
{
    // Create FIFO if it doesn't exist
    // mkfifo returns 0 on success, -1 on error (but EEXIST is OK)
    if (mkfifo(fifo_path.c_str(), 0666) != 0) {
        if (errno != EEXIST) {
            throw std::runtime_error(
                "Failed to create FIFO " + fifo_path + ": " + strerror(errno)
            );
        }
        // FIFO already exists, that's fine
    }
    
    // Open FIFO for writing with non-blocking flag
    // This allows the program to start even if no reader is connected yet
    fifo_fd_ = ::open(fifo_path.c_str(), O_WRONLY | O_NONBLOCK);
    if (fifo_fd_ < 0) {
        if (errno == ENXIO) {
            // No reader yet - this is OK, we'll retry on first write
            // For now, we'll accept this and retry when sending events
            std::cout << "FIFO created: " << fifo_path 
                      << " (waiting for reader to connect...)" << std::endl;
            // Don't throw - allow program to start, will connect when reader opens
            // We'll handle this in send_event()
        } else {
            throw std::runtime_error(
                "Failed to open FIFO " + fifo_path + ": " + strerror(errno)
            );
        }
    } else {
        std::cout << "FIFO opened: " << fifo_path << std::endl;
    }
}

// Destructor: Release resource automatically (RAII)
FifoWriter::~FifoWriter() {
    if (fifo_fd_ >= 0) {
        ::close(fifo_fd_);
        std::cout << "FIFO closed: " << fifo_path_ << std::endl;
    } else {
        // FIFO was created but never opened (no reader connected)
        std::cout << "FIFO was never connected: " << fifo_path_ << std::endl;
    }
}

// Move constructor
FifoWriter::FifoWriter(FifoWriter&& other) noexcept
    : fifo_fd_(other.fifo_fd_)
    , fifo_path_(std::move(other.fifo_path_))
{
    other.fifo_fd_ = -1;  // Prevent double-close in other's destructor
}

// Move assignment
FifoWriter& FifoWriter::operator=(FifoWriter&& other) noexcept {
    if (this != &other) {
        // Close current resource
        if (fifo_fd_ >= 0) {
            ::close(fifo_fd_);
        }
        
        // Move from other
        fifo_fd_ = other.fifo_fd_;
        fifo_path_ = std::move(other.fifo_path_);
        other.fifo_fd_ = -1;  // Prevent double-close in other's destructor
    }
    return *this;
}

// Send event - retries opening FIFO if reader connects later
void FifoWriter::send_event(WakeWordEventType event_type) {
    // If FIFO wasn't opened (no reader when constructor ran), try to open it now
    if (fifo_fd_ < 0) {
        fifo_fd_ = ::open(fifo_path_.c_str(), O_WRONLY | O_NONBLOCK);
        if (fifo_fd_ < 0) {
            if (errno == ENXIO) {
                // Still no reader - skip event (reader not connected yet)
                // Only log once per session to avoid spam
                static bool logged_no_reader = false;
                if (!logged_no_reader) {
                    std::cerr << "Warning: FIFO reader not connected, events will be dropped until reader connects" << std::endl;
                    logged_no_reader = true;
                }
                return;
            } else {
                throw std::runtime_error(
                    "Failed to open FIFO " + fifo_path_ + ": " + strerror(errno)
                );
            }
        } else {
            std::cout << "FIFO connected: " << fifo_path_ << std::endl;
        }
    }
    
    std::string json = event_to_json(event_type);
    
    // Write to FIFO
    ssize_t written = write(fifo_fd_, json.c_str(), json.size());
    if (written < 0) {
        if (errno == EPIPE) {
            // Reader closed - mark as disconnected, will retry next time
            ::close(fifo_fd_);
            fifo_fd_ = -1;
            std::cerr << "Warning: FIFO reader disconnected, event dropped" << std::endl;
            return;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Buffer full - this shouldn't happen with FIFOs, but handle it
            std::cerr << "Warning: FIFO buffer full, event dropped" << std::endl;
            return;
        } else {
            throw std::runtime_error(
                "Failed to write to FIFO: " + std::string(strerror(errno))
            );
        }
    }
    
    // Check if partial write (shouldn't happen with small JSON, but be safe)
    if (static_cast<size_t>(written) < json.size()) {
        std::cerr << "Warning: Partial write to FIFO (" << written 
                  << " of " << json.size() << " bytes)" << std::endl;
    }
    
    // Flush to ensure data is sent immediately
    fsync(fifo_fd_);
}

std::string FifoWriter::event_to_json(WakeWordEventType event_type) const {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"" 
        << (event_type == WakeWordEventType::START ? "START" : "STOP") 
        << "\""
        << "}\n";  // Include newline for readline()
    return oss.str();
}
