#pragma once

#include <cstddef>
#include <vector>
#include <cstdint>

// Forward declarations for library types
struct Fvad;
struct specbleach_denoiser;

// Audio filter combining VAD and noise reduction
class AudioFilter {
public:
    AudioFilter();
    ~AudioFilter();
    
    // Initialize VAD
    // sample_rate: Audio sample rate (e.g., 16000)
    // vad_mode: VAD aggressiveness (0=quality, 1=low bitrate, 2=aggressive, 3=very aggressive)
    bool init_vad(int sample_rate, int vad_mode);
    
    // Initialize noise reduction
    // sample_rate: Audio sample rate
    // reduction_amount: Noise reduction strength (0.0-1.0)
    bool init_noise_reduction(int sample_rate, float reduction_amount);
    
    // Process audio samples
    // samples: Input/output audio samples (modified in-place)
    // sample_count: Number of samples
    // sample_rate: Audio sample rate
    // Returns: true if speech detected (samples processed), false if discarded
    bool process_audio(float* samples, size_t sample_count, int sample_rate);
    
    // Cleanup resources
    void cleanup();
    
    // Check if VAD is enabled
    bool is_vad_enabled() const { return vad_enabled_; }
    
    // Check if noise reduction is enabled
    bool is_noise_reduction_enabled() const { return noise_reduction_enabled_; }

private:
    Fvad* vad_context_;
    specbleach_denoiser* noise_reduction_context_;
    bool vad_enabled_;
    bool noise_reduction_enabled_;
    int sample_rate_;
    
    // Temporary buffer for int16_t conversion (libfvad uses int16_t)
    std::vector<int16_t> int16_buffer_;
    
    // Simple energy-based VAD (fallback if library not available)
    bool simple_vad_detect(const float* samples, size_t sample_count);
    
    // Simple noise reduction (fallback if library not available)
    void simple_noise_reduction(float* samples, size_t sample_count);
};

