#pragma once

#include <cstddef>
#include <vector>
#include <cstdint>

// Forward declarations for library types
// We use void* to avoid needing the actual types in the header
// The implementation will cast to the proper types when libraries are available

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
    void* vad_context_;  // libfvad context (Fvad* when HAVE_FVAD is defined)
    void* noise_reduction_context_;  // libspecbleach context (struct specbleach_denoiser* when HAVE_SPECBLEACH is defined)
    bool vad_enabled_;
    bool noise_reduction_enabled_;
    bool use_libraries_;  // Flag to track if libraries are available
    int sample_rate_;
    
    // Temporary buffer for int16_t conversion (libfvad uses int16_t)
    std::vector<int16_t> int16_buffer_;
    
    // Simple energy-based VAD (fallback if library not available)
    bool simple_vad_detect(const float* samples, size_t sample_count);
    
    // Simple noise reduction (fallback if library not available)
    void simple_noise_reduction(float* samples, size_t sample_count);
};

