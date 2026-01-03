#pragma once

#include <SDL.h>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <stdexcept>

#include "audio_utils.hpp"
#include "iaudio_capture.hpp"
 
// Forward declaration
class AudioFilter;

class AudioCapture : public IAudioCapture {
public:
    // Constructor initializes SDL and opens the device
    // Throws std::runtime_error on failure
    explicit AudioCapture(int target_sample_rate = 16000);
    
    ~AudioCapture() override;

    // Start audio capture
    void start() override;

    // Stop audio capture
    void stop() override;

    // Cleanup audio system
    // Called automatically by destructor, but can be called manually
    void cleanup();

    // Get the audio queue to consume data
    AudioQueue<float>& get_audio_queue() override { return audio_queue_; }

    // Set audio filter for callback (can be nullptr)
    void set_audio_filter(AudioFilter* filter) override;

    // Set verbose mode flag
    void set_verbose(bool verbose) override;
    
    // Get current audio format
    const AudioFormat& get_format() const override { return format_; }
    
    // Helper used by static callback
    void process_audio(const float* samples, int frame_count);

private:
    SDL_AudioDeviceID dev_;
    AudioFormat format_;
    AudioQueue<float> audio_queue_;
    AudioFilter* filter_;
    std::atomic<bool> is_recording_;
    std::atomic<bool> verbose_;
    bool sdl_initialized_; // Track if we initialized SDL
};

