#pragma once

#include "args.hpp"
#include "channel.hpp"
#include "wake_word_event.hpp"
#include <SDL.h>
#include <string>
#include <atomic>
#include <thread>
#include <memory>

// Forward declarations
class WhisperTranscriber;
class AudioFilter;

// Wake word detector class
// Manages the lifecycle of wake word detection: initialization, processing, and cleanup
class WakeWordDetector {
public:
    WakeWordDetector();
    ~WakeWordDetector();
    
    // Initialize components (whisper, audio filter, audio capture)
    // event_channel: Channel for sending START/STOP events
    // Returns true on success, false on error
    bool initialize(const Config& config, Channel<WakeWordEvent>& event_channel);
    
    // Start wake word detection
    // Returns 0 on success, non-zero on error
    int start();
    
    // Stop wake word detection
    void stop();
    
    // Cleanup all resources
    void cleanup();

private:
    // Setup and open audio device, print info
    // Returns true on success, false on failure
    bool setup_audio_device();
    
    // Process audio chunks in a loop (runs in separate thread)
    void process_audio_chunks();
    
    // Resample audio chunk to target sample rate
    std::vector<float> resample_chunk(const std::vector<float>& chunk, 
                                      int target_sample_rate) const;
    
    // Handle transcription result (print and check for phrases)
    void handle_transcription(const std::string& transcription);

private:
    Config config_;
    std::unique_ptr<WhisperTranscriber> transcriber_;
    std::unique_ptr<AudioFilter> audio_filter_;
    SDL_AudioDeviceID dev_;
    std::atomic<bool> stop_requested_;
    std::atomic<bool> phrase_start_detected_;
    std::thread chunk_processor_thread_;
    bool initialized_;
    bool running_;
    Channel<WakeWordEvent>* event_channel_;  // Reference to channel for sending events
};

// Convenience function for backward compatibility
// Returns 0 on success, non-zero on error
int start_wake_word_detection(const Config& config);

