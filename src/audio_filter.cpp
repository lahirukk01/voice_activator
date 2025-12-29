#include "audio_filter.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdint>

// Try to include library headers
#ifdef HAVE_FVAD
#include <fvad.h>
#endif

#ifdef HAVE_SPECBLEACH
#include <specbleach_denoiser.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Simple energy-based VAD threshold
static const float VAD_ENERGY_THRESHOLD = 0.01f;

AudioFilter::AudioFilter()
    : vad_context_(nullptr)
    , noise_reduction_context_(nullptr)
    , vad_enabled_(false)
    , noise_reduction_enabled_(false)
    , use_libraries_(false)
    , sample_rate_(16000)
{
#ifdef HAVE_FVAD
#ifdef HAVE_SPECBLEACH
    use_libraries_ = true;
#endif
#endif
}

AudioFilter::~AudioFilter() {
    cleanup();
}

bool AudioFilter::init_vad(int sample_rate, int vad_mode) {
    sample_rate_ = sample_rate;
    vad_enabled_ = true;
    
#ifdef HAVE_FVAD
    Fvad* vad = fvad_new();
    if (vad == nullptr) {
        return false;
    }
    
    if (fvad_set_sample_rate(vad, sample_rate) < 0) {
        fvad_free(vad);
        return false;
    }
    
    if (fvad_set_mode(vad, vad_mode) < 0) {
        fvad_free(vad);
        return false;
    }
    
    vad_context_ = vad;
    return true;
#else
    // Fallback: using simple energy-based VAD
    vad_context_ = nullptr;
    return true;
#endif
}

bool AudioFilter::init_noise_reduction(int sample_rate, float reduction_amount) {
    sample_rate_ = sample_rate;
    noise_reduction_enabled_ = true;
    
#ifdef HAVE_SPECBLEACH
    struct specbleach_denoiser* denoiser = specbleach_denoiser_new();
    if (denoiser == nullptr) {
        return false;
    }
    
    specbleach_denoiser_set_sample_rate(denoiser, sample_rate);
    
    // Set noise reduction amount (0.0-1.0 maps to library's parameters)
    // libspecbleach uses different parameters, we'll use a reasonable default
    // The reduction_amount can be used to adjust processing intensity
    
    noise_reduction_context_ = denoiser;
    return true;
#else
    // Fallback: using simple high-pass filter
    noise_reduction_context_ = nullptr;
    return true;
#endif
}

bool AudioFilter::simple_vad_detect(const float* samples, size_t sample_count) {
    if (sample_count == 0) return false;
    
    // Calculate RMS energy
    float energy = 0.0f;
    for (size_t i = 0; i < sample_count; i++) {
        energy += samples[i] * samples[i];
    }
    energy = std::sqrt(energy / sample_count);
    
    // Simple threshold-based detection
    return energy > VAD_ENERGY_THRESHOLD;
}

void AudioFilter::simple_noise_reduction(float* samples, size_t sample_count) {
    if (sample_count < 2) return;
    
    // Simple high-pass filter to remove low-frequency noise
    // First-order IIR high-pass filter with cutoff ~80 Hz
    const float cutoff = 80.0f;
    const float rc = 1.0f / (2.0f * M_PI * cutoff);
    const float dt = 1.0f / sample_rate_;
    const float alpha = dt / (rc + dt);
    
    float y = samples[0];
    for (size_t i = 1; i < sample_count; i++) {
        y = alpha * (y + samples[i] - samples[i - 1]);
        samples[i] = y;
    }
}

bool AudioFilter::process_audio(float* samples, size_t sample_count, int sample_rate) {
    if (sample_count == 0) return false;
    
    // Step 1: VAD detection
    if (vad_enabled_) {
        bool has_speech = false;
        
#ifdef HAVE_FVAD
        if (vad_context_ != nullptr) {
            Fvad* vad = static_cast<Fvad*>(vad_context_);
            // libfvad expects int16_t samples
            // Convert float samples to int16_t
            if (int16_buffer_.size() < sample_count) {
                int16_buffer_.resize(sample_count);
            }
            
            for (size_t i = 0; i < sample_count; i++) {
                // Clamp and convert float [-1.0, 1.0] to int16_t
                float clamped = std::max(-1.0f, std::min(1.0f, samples[i]));
                int16_buffer_[i] = static_cast<int16_t>(clamped * 32767.0f);
            }
            
            // Process with libfvad (expects frame size, typically 10ms = 160 samples at 16kHz)
            // Process in chunks if needed
            int result = fvad_process(vad, int16_buffer_.data(), static_cast<int>(sample_count));
            has_speech = (result > 0);
        } else {
            has_speech = simple_vad_detect(samples, sample_count);
        }
#else
        has_speech = simple_vad_detect(samples, sample_count);
#endif
        
        if (!has_speech) {
            return false;  // No speech, discard samples
        }
    }
    
    // Step 2: Noise reduction (only if speech detected)
    if (noise_reduction_enabled_) {
#ifdef HAVE_SPECBLEACH
        if (noise_reduction_context_ != nullptr) {
            struct specbleach_denoiser* denoiser = static_cast<struct specbleach_denoiser*>(noise_reduction_context_);
            // libspecbleach processes float samples in-place
            specbleach_denoiser_process(denoiser, samples, samples, static_cast<int>(sample_count));
        } else {
            simple_noise_reduction(samples, sample_count);
        }
#else
        simple_noise_reduction(samples, sample_count);
#endif
    }
    
    return true;  // Speech detected and processed
}

void AudioFilter::cleanup() {
#ifdef HAVE_FVAD
    if (vad_context_ != nullptr) {
        Fvad* vad = static_cast<Fvad*>(vad_context_);
        fvad_free(vad);
        vad_context_ = nullptr;
    }
#endif

#ifdef HAVE_SPECBLEACH
    if (noise_reduction_context_ != nullptr) {
        struct specbleach_denoiser* denoiser = static_cast<struct specbleach_denoiser*>(noise_reduction_context_);
        specbleach_denoiser_free(denoiser);
        noise_reduction_context_ = nullptr;
    }
#endif

    vad_enabled_ = false;
    noise_reduction_enabled_ = false;
}

