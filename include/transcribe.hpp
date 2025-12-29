#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>

// Forward declaration
struct whisper_context;

// WhisperTranscriber class for managing whisper context and chunk-wise transcription
class WhisperTranscriber {
public:
    WhisperTranscriber();
    ~WhisperTranscriber();
    
    // Initialize whisper asynchronously in a background thread
    // verbose: If true, show whisper initialization logs; if false, suppress them
    void init_async(const std::string& model_path, bool verbose = false);
    
    // Check if whisper is ready
    bool is_ready() const;
    
    // Wait until whisper is ready (blocks)
    void wait_for_ready();
    
    // Transcribe a single audio chunk
    // Returns transcribed text, or empty string on error
    std::string transcribe_chunk(const std::vector<float>& audio_data);
    
    // Check if text contains start or stop phrase
    // Returns: 1 for start phrase, -1 for stop phrase, 0 for neither
    int check_phrases(const std::string& text, const std::string& start_phrase, const std::string& stop_phrase);
    
    // Cleanup resources
    void cleanup();

private:
    whisper_context* ctx_;
    std::mutex ctx_mutex_;
    std::atomic<bool> is_ready_;
    std::atomic<bool> init_failed_;
    std::thread init_thread_;
    
    void init_whisper_internal(const std::string& model_path, bool verbose);
};

