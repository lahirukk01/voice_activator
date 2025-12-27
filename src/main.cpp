#include <SDL.h>
#include <vector>
#include <iostream>
#include <atomic>
#include <string>
#include <thread>
#include <algorithm>

#include "args.h"
#include "transcribe.h"
#include "audio_filter.h"

// Buffer to store audio (will be converted to 16kHz mono for whisper)
std::vector<float> g_audio_buffer;
std::atomic<bool> g_is_recording(false);

// Global audio filter instance (accessed from callback)
AudioFilter* g_audio_filter = nullptr;

// Actual audio format from SDL
struct AudioFormat {
    int sample_rate = 16000;
    int channels = 1;
    bool is_stereo = false;
} g_audio_format;

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
    }
    
    return dev;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    Config config = parse_args(argc, argv);
    
    if (config.show_help) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (!config.valid) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Initialize whisper asynchronously early
    WhisperTranscriber transcriber;
    transcriber.init_async(config.model_path, config.verbose);
    
    // Initialize audio filter (VAD + noise reduction)
    AudioFilter audio_filter;
    const int sample_rate = 16000;
    if (config.enable_vad) {
        audio_filter.init_vad(sample_rate, config.vad_mode);
    }
    if (config.enable_noise_reduction) {
        audio_filter.init_noise_reduction(sample_rate, config.noise_reduction_amount);
    }
    g_audio_filter = &audio_filter;  // Set global pointer for callback
    
    if (SDL_Init(SDL_INIT_AUDIO) < 0) return -1;

    // Open default capture device (iscapture = 1)
    SDL_AudioSpec obtained;
    SDL_AudioDeviceID dev = open_audio_device(&obtained);
    
    if (dev == 0) {
        std::cerr << "Failed to open mic: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Store actual audio format
    g_audio_format.sample_rate = obtained.freq;
    g_audio_format.channels = obtained.channels;
    g_audio_format.is_stereo = (obtained.channels > 1);

    // Print audio device info
    std::cout << "Audio device opened:" << std::endl;
    std::cout << "  Sample rate: " << obtained.freq << " Hz" << std::endl;
    std::cout << "  Format: " << (obtained.format == AUDIO_F32 ? "F32" : "Other") << std::endl;
    std::cout << "  Channels: " << static_cast<int>(obtained.channels) << std::endl;
    std::cout << "  Buffer size: " << obtained.samples << " samples" << std::endl;
    
    // Check if format matches what we need
    const int target_sample_rate = 16000;
    if (obtained.freq != target_sample_rate) {
        std::cout << "  Warning: Sample rate mismatch (expected " << target_sample_rate 
                  << " Hz, got " << obtained.freq << " Hz)" << std::endl;
        std::cout << "  Audio will be resampled to " << target_sample_rate << " Hz for transcription" << std::endl;
    }
    if (g_audio_format.is_stereo) {
        std::cout << "  Note: Converting stereo to mono" << std::endl;
    }
    
    std::cout << "VAD: " << (config.enable_vad ? "enabled" : "disabled") << std::endl;
    std::cout << "Noise reduction: " << (config.enable_noise_reduction ? "enabled" : "disabled") << std::endl;
    std::cout << "Recording... Press Enter to stop." << std::endl;
    g_is_recording = true;
    SDL_PauseAudioDevice(dev, 0); // Start the stream

    // Chunk processing variables - use actual sample rate for buffer calculations
    // But whisper needs 16kHz, so we'll resample chunks before transcription
    const int whisper_sample_rate = 16000;
    const size_t chunk_size_samples = static_cast<size_t>(config.chunk_size_seconds * g_audio_format.sample_rate);
    const size_t overlap_samples = static_cast<size_t>(0.2 * g_audio_format.sample_rate);  // 0.2s overlap
    size_t last_processed_index = 0;
    std::atomic<bool> stop_requested(false);
    std::atomic<bool> phrase_start_detected(false);
    
    // Start a thread to process chunks while recording
    std::thread chunk_processor([&]() {
        // Wait for whisper to be ready
        transcriber.wait_for_ready();
        
        while (!stop_requested.load()) {
            // Wait for enough audio to be captured
            size_t current_size = g_audio_buffer.size();
            size_t required_size = last_processed_index + chunk_size_samples;
            
            if (current_size < required_size) {
                // Not enough audio yet, wait a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            // Extract chunk with overlap
            size_t chunk_start = (last_processed_index >= overlap_samples) 
                ? last_processed_index - overlap_samples 
                : 0;
            size_t chunk_end = last_processed_index + chunk_size_samples;
            chunk_end = std::min(chunk_end, current_size);
            
            if (chunk_end <= chunk_start) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            // Extract chunk
            std::vector<float> chunk(
                g_audio_buffer.begin() + chunk_start,
                g_audio_buffer.begin() + chunk_end
            );
            
            // Resample to 16kHz if needed (simple linear interpolation)
            std::vector<float> resampled_chunk;
            if (g_audio_format.sample_rate != whisper_sample_rate) {
                size_t target_size = static_cast<size_t>(chunk.size() * whisper_sample_rate / g_audio_format.sample_rate);
                resampled_chunk.reserve(target_size);
                
                double ratio = static_cast<double>(g_audio_format.sample_rate) / whisper_sample_rate;
                for (size_t i = 0; i < target_size; i++) {
                    double src_index = i * ratio;
                    size_t idx0 = static_cast<size_t>(src_index);
                    size_t idx1 = std::min(idx0 + 1, chunk.size() - 1);
                    double frac = src_index - idx0;
                    
                    float sample = chunk[idx0] * (1.0f - frac) + chunk[idx1] * frac;
                    resampled_chunk.push_back(sample);
                }
                chunk = resampled_chunk;
            }
            
            // Transcribe chunk (now at 16kHz)
            std::string transcription = transcriber.transcribe_chunk(chunk);
            
            if (!transcription.empty()) {
                // Check for phrases
                int phrase_result = transcriber.check_phrases(
                    transcription, 
                    config.start_phrase, 
                    config.stop_phrase
                );
                
                // Print if verbose mode OR phrase detected
                if (phrase_result == 1 && !phrase_start_detected.load()) {
                    phrase_start_detected.store(true);
                    std::cout << "[START PHRASE DETECTED] " << transcription << std::endl;
                } else if (phrase_result == -1 && phrase_start_detected.load()) {
                    std::cout << "[STOP PHRASE DETECTED] " << transcription << std::endl;
                    phrase_start_detected.store(false);
                }

                // Always print transcriptions
                std::cout << "[TRANSCRIPTION] " << transcription << std::endl;
                
            } else if (config.verbose) {
                // Debug: show when chunks are processed but return empty
                std::cout << "[DEBUG] Chunk processed but transcription empty (chunk size: " 
                          << chunk.size() << " samples)" << std::endl;
            }
            
            // Update last processed index (move forward by chunk size minus overlap)
            last_processed_index += (chunk_size_samples - overlap_samples);
        }
    });

    // Wait for user input or stop phrase
    std::cin.get(); // Wait for user

    // Signal stop
    stop_requested = true;
    SDL_PauseAudioDevice(dev, 1); // Stop
    g_is_recording = false;
    
    // Wait for chunk processor to finish
    if (chunk_processor.joinable()) {
        chunk_processor.join();
    }
    
    std::cout << "Captured " << g_audio_buffer.size() << " samples." << std::endl;

    SDL_CloseAudioDevice(dev);
    SDL_Quit();

    // Cleanup transcriber
    transcriber.cleanup();
    
    // Cleanup audio filter
    audio_filter.cleanup();
    g_audio_filter = nullptr;

    return 0;
}
