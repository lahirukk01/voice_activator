#pragma once

#include "iaudio_capture.hpp"
#include "itranscriber.hpp"
#include "channel.hpp"
#include "wake_word_event.hpp"
#include <SDL.h>
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <optional>

// Wake word detector class
// Manages the lifecycle of wake word detection: processing and event dispatching
// Dependencies (AudioCapture, Transcriber) are injected and must outlive this object.
class WakeWordDetector {
public:
    // Constructor receives injected dependencies
    WakeWordDetector(
        IAudioCapture& audio_capture,
        ITranscriber& transcriber,
        double chunk_size_seconds,
        const std::string& start_phrase,
        const std::string& stop_phrase,
        bool verbose,
        Channel<WakeWordEvent>& event_channel
    );
    
    ~WakeWordDetector();
    
    // Start wake word detection
    // Returns 0 on success, non-zero on error
    int start();
    
    // Stop wake word detection
    void stop();
    
    // Cleanup all resources
    void cleanup();

private:
    // Process audio chunks in a loop (runs in separate thread)
    void process_audio_chunks();
    
    // Resample audio chunk to target sample rate
    std::vector<float> resample_chunk(const std::vector<float>& chunk, 
                                      int target_sample_rate) const;
    
    // Handle transcription result (print and check for phrases)
    void handle_transcription(const std::string& transcription);

private:
    // Dependencies
    IAudioCapture& audio_capture_;
    ITranscriber& transcriber_;
    
    // Configuration
    double chunk_size_seconds_;
    std::string start_phrase_;
    std::string stop_phrase_;
    bool verbose_;

    std::atomic<bool> stop_requested_;
    std::atomic<bool> phrase_start_detected_;
    std::thread chunk_processor_thread_;
    bool running_;
    Channel<WakeWordEvent>& event_channel_; // Reference to channel for sending events
};




