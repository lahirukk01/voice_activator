#include <SDL.h>
#include <vector>
#include <iostream>
#include <atomic>
#include <string>
#include <thread>
#include <algorithm>

#include "args.h"
#include "transcribe.h"

// Buffer to store 16kHz mono float32 audio
std::vector<float> g_audio_buffer;
std::atomic<bool> g_is_recording(false);

// Callback: Runs on a background thread managed by macOS CoreAudio
void audio_callback(void* userdata, Uint8* stream, int len) {
    if (!g_is_recording) return;

    // SDL provides bytes; cast to float for Whisper compatibility
    float* samples = reinterpret_cast<float*>(stream);
    size_t sample_count = len / sizeof(float);

    g_audio_buffer.insert(g_audio_buffer.end(), samples, samples + sample_count);
}

SDL_AudioDeviceID open_audio_device() {
    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    desired.freq = 16000;       // Target: 16kHz
    desired.format = AUDIO_F32; // Whisper wants 32-bit floats
    desired.channels = 1;       // Mono
    desired.samples = 4096;
    desired.callback = audio_callback;

    // Open default capture device (iscapture = 1)
    return SDL_OpenAudioDevice(nullptr, 1, &desired, &obtained, 0);
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
    transcriber.init_async(config.model_path);
    
    if (SDL_Init(SDL_INIT_AUDIO) < 0) return -1;

    // Open default capture device (iscapture = 1)
    SDL_AudioDeviceID dev = open_audio_device();
    
    if (dev == 0) {
        std::cerr << "Failed to open mic: " << SDL_GetError() << std::endl;
        return 1;
    }

    std::cout << "Recording... Press Enter to stop." << std::endl;
    g_is_recording = true;
    SDL_PauseAudioDevice(dev, 0); // Start the stream

    // Chunk processing variables
    const int sample_rate = 16000;
    const size_t chunk_size_samples = static_cast<size_t>(config.chunk_size_seconds * sample_rate);
    const size_t overlap_samples = static_cast<size_t>(0.2 * sample_rate);  // 0.2s overlap
    size_t last_processed_index = 0;
    std::atomic<bool> stop_requested(false);
    std::atomic<bool> phrase_start_detected(false);
    
    // Start a thread to process chunks while recording
    std::thread chunk_processor([&]() {
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
            
            // Transcribe chunk
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

                if (config.verbose) {
                    std::cout << "[CHUNK] " << transcription << std::endl;
                }
                
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

    return 0;
}
