#include "transcribe.h"
#include "whisper.h"
#include <iostream>
#include <thread>
#include <regex>

WhisperTranscriber::WhisperTranscriber() 
    : ctx_(nullptr), is_ready_(false), init_failed_(false) {
}

WhisperTranscriber::~WhisperTranscriber() {
    cleanup();
}

void WhisperTranscriber::init_async(const std::string& model_path) {
    if (init_thread_.joinable()) {
        // Already initializing or initialized
        return;
    }
    
    is_ready_ = false;
    init_failed_ = false;
    
    init_thread_ = std::thread(&WhisperTranscriber::init_whisper_internal, this, model_path);
}

void WhisperTranscriber::init_whisper_internal(const std::string& model_path) {
    std::cout << "Initializing whisper model (async)..." << std::endl;
    
    whisper_context_params cparams = whisper_context_default_params();
    whisper_context* new_ctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    
    std::lock_guard<std::mutex> lock(ctx_mutex_);
    
    if (new_ctx == nullptr) {
        std::cerr << "Error: Failed to initialize whisper context from model: " << model_path << std::endl;
        init_failed_ = true;
        return;
    }
    
    ctx_ = new_ctx;
    is_ready_ = true;
    std::cout << "Whisper model loaded successfully!" << std::endl;
}

bool WhisperTranscriber::is_ready() const {
    return is_ready_.load();
}

void WhisperTranscriber::wait_for_ready() {
    if (init_thread_.joinable()) {
        init_thread_.join();
    }
    
    if (init_failed_.load()) {
        throw std::runtime_error("Whisper initialization failed");
    }
}

std::string WhisperTranscriber::transcribe_chunk(const std::vector<float>& audio_data) {
    if (audio_data.empty()) {
        return "";
    }
    
    if (!is_ready_.load()) {
        wait_for_ready();
    }
    
    if (init_failed_.load() || ctx_ == nullptr) {
        return "";
    }
    
    std::lock_guard<std::mutex> lock(ctx_mutex_);
    
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
    
    // Run transcription
    if (whisper_full(ctx_, wparams, audio_data.data(), audio_data.size()) != 0) {
        std::cerr << "Error: Failed to process audio chunk" << std::endl;
        return "";
    }
    
    // Extract transcription text
    std::string result;
    const int n_segments = whisper_full_n_segments(ctx_);
    
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx_, i);
        result += text;
    }
    
    return result;
}

int WhisperTranscriber::check_phrases(const std::string& text, const std::string& start_phrase, const std::string& stop_phrase) {
    try {
        // Case-insensitive regex matching
        std::regex start_regex(start_phrase, std::regex_constants::icase);
        std::regex stop_regex(stop_phrase, std::regex_constants::icase);
        
        if (std::regex_search(text, start_regex)) {
            return 1;  // Start phrase detected
        }
        if (std::regex_search(text, stop_regex)) {
            return -1;  // Stop phrase detected
        }
    } catch (const std::regex_error& e) {
        std::cerr << "Regex error: " << e.what() << std::endl;
    }
    
    return 0;  // No phrase detected
}

void WhisperTranscriber::cleanup() {
    if (init_thread_.joinable()) {
        init_thread_.join();
    }
    
    std::lock_guard<std::mutex> lock(ctx_mutex_);
    if (ctx_ != nullptr) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
    is_ready_ = false;
}

