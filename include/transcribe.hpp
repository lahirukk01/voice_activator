#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>

#include "itranscriber.hpp"

// Forward declaration
struct whisper_context;

// WhisperTranscriber class for managing whisper context and chunk-wise transcription
class WhisperTranscriber : public ITranscriber {
public:
    WhisperTranscriber();
    ~WhisperTranscriber() override;
    
    // Initialize whisper asynchronously in a background thread
    // verbose: If true, show whisper initialization logs; if false, suppress them
    void init_async(const std::string& model_path, bool verbose = false);
    
    // Check if whisper is ready
    bool is_ready() const;
    
    // Wait until whisper is ready (blocks)
    void wait_for_ready() override;
    
    // Transcribe a single audio chunk
    // Returns transcribed text, or empty string on error
    std::string transcribe_chunk(const std::vector<float>& audio_data) override;
    
    // Check if text contains start or stop phrase
    // Returns: 1 for start phrase, -1 for stop phrase, 0 for neither
    int check_phrases(const std::string& text, const std::string& start_phrase, const std::string& stop_phrase) override;
    
    // Cleanup resources
    void cleanup();

private:
    // Custom deleter for whisper_context
    struct WhisperFree {
        void operator()(whisper_context* ctx);
    };
    
    std::unique_ptr<whisper_context, WhisperFree> ctx_;
    std::mutex ctx_mutex_;
    std::atomic<bool> is_ready_;
    std::atomic<bool> init_failed_;
    std::thread init_thread_;
    
    void init_whisper_internal(const std::string& model_path, bool verbose);
};

