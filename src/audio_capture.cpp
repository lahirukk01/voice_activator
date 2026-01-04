#include "audio_capture.hpp"
#include "audio_filter.hpp"
#include <SDL.h>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>

// Callback: Runs on a background thread managed by macOS CoreAudio
static void audio_callback_proxy(void* userdata, Uint8* stream, int len) {
    AudioCapture* capture = static_cast<AudioCapture*>(userdata);
    if (capture) {
        // SDL provides bytes; cast to float
        float* samples = reinterpret_cast<float*>(stream);
        size_t frame_count = len / sizeof(float);
        capture->process_audio(samples, frame_count);
    }
}

AudioCapture::AudioCapture(int target_sample_rate)
    : dev_(0)
    , filter_(nullptr)
    , is_recording_(false)
    , verbose_(false)
    , sdl_initialized_(false)
{
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    sdl_initialized_ = true;

    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    desired.freq = target_sample_rate;
    desired.format = AUDIO_F32; // Whisper wants 32-bit floats
    desired.channels = 1;       // Mono
    desired.samples = 4096;
    desired.callback = audio_callback_proxy;
    desired.userdata = this;    // Pass instance to callback

    // Open default capture device (iscapture = 1)
    dev_ = SDL_OpenAudioDevice(nullptr, 1, &desired, &obtained, 0);
    
    if (dev_ == 0) {
        throw std::runtime_error(std::string("Failed to open audio device: ") + SDL_GetError());
    }
    
    // Store actual audio format
    format_.sample_rate = obtained.freq;
    format_.channels = obtained.channels;
    format_.is_stereo = (obtained.channels > 1);
}

AudioCapture::~AudioCapture() {
    cleanup();
}

void AudioCapture::start() {
    if (dev_ != 0) {
        is_recording_.store(true);
        SDL_PauseAudioDevice(dev_, 0); // Start the stream
    }
}

void AudioCapture::stop() {
    if (dev_ != 0) {
        is_recording_.store(false);
        SDL_PauseAudioDevice(dev_, 1); // Stop
    }
}

void AudioCapture::cleanup() {
    stop();
    if (dev_ != 0) {
        SDL_CloseAudioDevice(dev_);
        dev_ = 0;
    }
    if (sdl_initialized_) {
        SDL_Quit();
        sdl_initialized_ = false;
    }
}

void AudioCapture::set_audio_filter(AudioFilter* filter) {
    filter_ = filter;
}

void AudioCapture::set_verbose(bool verbose) {
    verbose_.store(verbose);
}

void AudioCapture::process_audio(const float* samples, int frame_count) {
    if (!is_recording_.load()) return;

    // Convert stereo to mono if needed
    std::vector<float> processed_samples;
    
    // We need a non-const buffer for processing if we're not converting
    std::vector<float> scratch_buffer;
    float* samples_to_process = const_cast<float*>(samples);
    size_t samples_to_process_count = frame_count;

    if (format_.is_stereo) {
        size_t mono_count = frame_count / 2;
        processed_samples.reserve(mono_count);
        for (size_t i = 0; i < mono_count; i++) {
            float left = samples[i * 2];
            float right = samples[i * 2 + 1];
            processed_samples.push_back((left + right) / 2.0f);
        }
        samples_to_process = processed_samples.data();
        samples_to_process_count = mono_count;
    } else {
        // If mono, we might need a copy if we are going to modify it in place with filter
        // But AudioFilter::process_audio modifies in place
        // Since SDL gives us a buffer we shouldn't modify if it's stereo (we handled that),
        // but if it's mono, it's the raw stream. 
        // Safer to copy to scratch buffer for processing to avoid messing with SDL internal buffer if unintended
        scratch_buffer.assign(samples, samples + frame_count);
        samples_to_process = scratch_buffer.data();
    }

    // Process: VAD + Noise Reduction
    bool samples_accepted = true;
    if (filter_ != nullptr && 
        (filter_->is_vad_enabled() || filter_->is_noise_reduction_enabled())) {
        
        samples_accepted = filter_->process_audio(samples_to_process, samples_to_process_count, format_.sample_rate);
        
        if (!samples_accepted) {
            // VAD filtered out
            if (verbose_.load()) {
                static size_t filtered_count = 0;
                static auto last_report = std::chrono::steady_clock::now();
                filtered_count += samples_to_process_count;
                
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - last_report).count() >= 5) {
                    std::cout << "[DEBUG] VAD filtered " << filtered_count << " samples" << std::endl;
                    filtered_count = 0;
                    last_report = now;
                }
            }
            return; 
        }
    }

    // Add to thread-safe queue
    // We push the vector we just processed
    if (samples_to_process == processed_samples.data()) {
         audio_queue_.push(processed_samples);
    } else {
         std::vector<float> final_samples(samples_to_process, samples_to_process + samples_to_process_count);
         audio_queue_.push(final_samples);
    }
    
    // Debug report
    if (verbose_.load()) {
        static auto last_buffer_report = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_buffer_report).count() >= 5) {
            std::cout << "[DEBUG] Audio queue size: " << audio_queue_.size() << " chunks" << std::endl;
            last_buffer_report = now;
        }
    }
}

