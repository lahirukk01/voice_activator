#include "transcribe.h"
#include "whisper.h"
#include <iostream>
#include <thread>

std::string transcribe_audio(const std::vector<float>& audio_data, const std::string& model_path) {
    if (audio_data.empty()) {
        std::cerr << "Error: Audio data is empty" << std::endl;
        return "";
    }

    // Initialize whisper context
    whisper_context_params cparams = whisper_context_default_params();
    struct whisper_context* ctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    
    if (ctx == nullptr) {
        std::cerr << "Error: Failed to initialize whisper context from model: " << model_path << std::endl;
        return "";
    }

    // Set up transcription parameters
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    
    // Get maximum number of threads (leave 2 cores free)
    const int max_threads = std::max(1, std::min(8, (int)std::thread::hardware_concurrency() - 2));
    
    wparams.print_progress   = false;
    wparams.print_realtime   = false;
    wparams.print_timestamps = false;
    wparams.print_special    = false;
    wparams.translate        = false;
    wparams.language         = "en";
    wparams.n_threads        = max_threads;
    wparams.offset_ms        = 0;
    wparams.no_context       = true;
    wparams.single_segment   = false;

    std::cout << "Transcribing audio with " << max_threads << " threads..." << std::endl;

    // Run transcription
    if (whisper_full(ctx, wparams, audio_data.data(), audio_data.size()) != 0) {
        std::cerr << "Error: Failed to process audio" << std::endl;
        whisper_free(ctx);
        return "";
    }

    // Extract transcription text
    std::string result;
    const int n_segments = whisper_full_n_segments(ctx);
    
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx, i);
        result += text;
    }

    whisper_free(ctx);
    
    return result;
}

