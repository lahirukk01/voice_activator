#include "audio_capture.hpp"
#include "audio_filter.hpp"
#include <SDL.h>
#include <vector>
#include <atomic>

// Global audio buffer and state
std::vector<float> g_audio_buffer;
std::atomic<bool> g_is_recording(false);
AudioFormat g_audio_format;

// Global audio filter instance (accessed from callback)
static AudioFilter* g_audio_filter = nullptr;

// Callback: Runs on a background thread managed by macOS CoreAudio
void audio_callback(void* userdata, Uint8* stream, int len) {
    if (!g_is_recording) return;

    // SDL provides bytes; cast to float
    float* samples = reinterpret_cast<float*>(stream);
    size_t frame_count = len / sizeof(float);
    
    // Convert stereo to mono if needed (average left and right channels)
    std::vector<float> mono_samples;
    if (g_audio_format.is_stereo) {
        size_t mono_count = frame_count / 2;
        mono_samples.reserve(mono_count);
        for (size_t i = 0; i < mono_count; i++) {
            // Average left and right channels
            float left = samples[i * 2];
            float right = samples[i * 2 + 1];
            mono_samples.push_back((left + right) / 2.0f);
        }
        samples = mono_samples.data();
        frame_count = mono_count;
    }

    // Process: VAD + Noise Reduction (only if enabled)
    if (g_audio_filter != nullptr && 
        (g_audio_filter->is_vad_enabled() || g_audio_filter->is_noise_reduction_enabled())) {
        if (!g_audio_filter->process_audio(samples, frame_count, g_audio_format.sample_rate)) {
            return;  // No speech detected, discard samples
        }
        // Samples are now cleaned and contain speech
    }

    // Add samples to buffer
    g_audio_buffer.insert(g_audio_buffer.end(), samples, samples + frame_count);
}

bool init_audio_capture() {
    return SDL_Init(SDL_INIT_AUDIO) >= 0;
}

SDL_AudioDeviceID open_audio_device(SDL_AudioSpec* obtained_spec) {
    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    desired.freq = 16000;       // Target: 16kHz
    desired.format = AUDIO_F32; // Whisper wants 32-bit floats
    desired.channels = 1;       // Mono
    desired.samples = 4096;
    desired.callback = audio_callback;

    // Open default capture device (iscapture = 1)
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 1, &desired, &obtained, 0);
    
    if (obtained_spec && dev != 0) {
        *obtained_spec = obtained;
        
        // Store actual audio format
        g_audio_format.sample_rate = obtained.freq;
        g_audio_format.channels = obtained.channels;
        g_audio_format.is_stereo = (obtained.channels > 1);
    }
    
    return dev;
}

void start_audio_capture(SDL_AudioDeviceID dev) {
    g_is_recording = true;
    SDL_PauseAudioDevice(dev, 0); // Start the stream
}

void stop_audio_capture(SDL_AudioDeviceID dev) {
    g_is_recording = false;
    SDL_PauseAudioDevice(dev, 1); // Stop
}

void cleanup_audio() {
    SDL_Quit();
}

void set_audio_filter(AudioFilter* filter) {
    g_audio_filter = filter;
}

