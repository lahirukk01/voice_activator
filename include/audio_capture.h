#pragma once

#include <SDL.h>
#include <vector>
#include <atomic>

// Audio format information
struct AudioFormat {
    int sample_rate = 16000;
    int channels = 1;
    bool is_stereo = false;
};

// Global audio buffer and state
extern std::vector<float> g_audio_buffer;
extern std::atomic<bool> g_is_recording;
extern AudioFormat g_audio_format;

// Forward declaration
class AudioFilter;

// Initialize audio capture system
bool init_audio_capture();

// Open audio device and return device ID
// Returns 0 on failure
SDL_AudioDeviceID open_audio_device(SDL_AudioSpec* obtained_spec);

// Start audio capture
void start_audio_capture(SDL_AudioDeviceID dev);

// Stop audio capture
void stop_audio_capture(SDL_AudioDeviceID dev);

// Cleanup audio system
void cleanup_audio();

// Set audio filter for callback (can be nullptr)
void set_audio_filter(class AudioFilter* filter);

