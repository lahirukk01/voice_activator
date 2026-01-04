#pragma once

#include <string>
#include <optional>

struct Config {
    std::string output_dir = "output";
    std::string custom_filename = "";  // Empty means use auto-generated timestamp
    std::string model_path = "";  // Default to base.en model
    double chunk_size_seconds = 3.0;  // Chunk size for transcription (default: 3 seconds)
    std::string start_phrase = "hey[!?.,]?\\s+alfred";
    std::string stop_phrase = "stop[!?.,]?\\s+alfred";
    bool verbose = false;
    
    // VAD settings (nullopt = disabled)
    std::optional<int> vad_mode = std::nullopt;
    
    // Noise reduction settings (nullopt = disabled)
    std::optional<float> noise_reduction_amount = std::nullopt;
    
    std::string fifo_path = "/tmp/wake_word_events";
    bool show_help = false;
    bool valid = true;  // false if parsing failed
};

// Parse command line arguments and return configuration
Config parse_args(int argc, char* argv[]);

// Print usage information
void print_usage(const char* program_name);
