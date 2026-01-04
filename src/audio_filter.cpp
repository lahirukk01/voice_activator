#include "audio_filter.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdint>

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
    , sample_rate_(16000)
{
}

AudioFilter::~AudioFilter() {
    cleanup();
}

bool AudioFilter::init_vad(int sample_rate, int vad_mode) {
    sample_rate_ = sample_rate;
    vad_enabled_ = true;
    
#ifdef HAVE_FVAD
    vad_context_ = fvad_new();
    if (vad_context_ == nullptr) {
        return false;
    }
    
    if (fvad_set_sample_rate(vad_context_, sample_rate) < 0) {
        fvad_free(vad_context_);
        vad_context_ = nullptr;
        return false;
    }
    
    if (fvad_set_mode(vad_context_, vad_mode) < 0) {
        fvad_free(vad_context_);
        vad_context_ = nullptr;
        return false;
    }
    
    return true;
#else
    // Fallback: using simple energy-based VAD
    return true;
#endif
}

bool AudioFilter::init_noise_reduction(int sample_rate, float reduction_amount) {
    sample_rate_ = sample_rate;
    noise_reduction_enabled_ = true;
    
#ifdef HAVE_SPECBLEACH
    noise_reduction_context_ = specbleach_denoiser_new();
    if (noise_reduction_context_ == nullptr) {
        return false;
    }
    
    specbleach_denoiser_set_sample_rate(noise_reduction_context_, sample_rate);
    
    // Set noise reduction amount (0.0-1.0 maps to library's parameters)
    // libspecbleach uses different parameters, we'll use a reasonable default
    // The reduction_amount can be used to adjust processing intensity
    
    return true;
#else
    // Fallback: using simple high-pass filter
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
            const size_t chunk_size_ms = 10;
            const size_t chunk_size_samples = (sample_rate * chunk_size_ms) / 1000;
            
            if (int16_buffer_.size() < chunk_size_samples) {
                int16_buffer_.resize(chunk_size_samples);
            }

            for (size_t i = 0; i < sample_count; i += chunk_size_samples) {
                const size_t remaining_samples = sample_count - i;
                const size_t current_chunk_size = std::min(chunk_size_samples, remaining_samples);

                for (size_t j = 0; j < current_chunk_size; ++j) {
                    float clamped = std::max(-1.0f, std::min(1.0f, samples[i + j]));
                    int16_buffer_[j] = static_cast<int16_t>(clamped * 32767.0f);
                }

                if (current_chunk_size > 0) {
                    int result = fvad_process(vad_context_, int16_buffer_.data(), current_chunk_size);
                    if (result > 0) {
                        has_speech = true;
                        break; 
                    }
                }
            }
        } else {
            has_speech = simple_vad_detect(samples, sample_count);
        }
#else
        has_speech = simple_vad_detect(samples, sample_count);
#endif
        
        if (!has_speech) {
            return false;
        }
    }
    
    // Step 2: Noise reduction
    if (noise_reduction_enabled_) {
#ifdef HAVE_SPECBLEACH
        if (noise_reduction_context_ != nullptr) {
            specbleach_denoiser_process(noise_reduction_context_, samples, samples, static_cast<int>(sample_count));
        } else {
            simple_noise_reduction(samples, sample_count);
        }
#else
        simple_noise_reduction(samples, sample_count);
#endif
    }
    
    return true;
}

void AudioFilter::cleanup() {
#ifdef HAVE_FVAD
    if (vad_context_ != nullptr) {
        fvad_free(vad_context_);
        vad_context_ = nullptr;
    }
#endif

#ifdef HAVE_SPECBLEACH
    if (noise_reduction_context_ != nullptr) {
        specbleach_denoiser_free(noise_reduction_context_);
        noise_reduction_context_ = nullptr;
    }
#endif

    vad_enabled_ = false;
    noise_reduction_enabled_ = false;
}

