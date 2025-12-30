#pragma once

#include <string>

struct Config {
    std::string output_dir = "output";
    std::string custom_filename = "";  // Empty means use auto-generated timestamp
    std::string model_path = "/Users/lahirukk/SoftwareProjects/Python/whisper.cpp/models/ggml-base.en.bin";  // Default to base.en model
    double chunk_size_seconds = 3.0;  // Chunk size for transcription (default: 3 seconds)
    std::string start_phrase = "hey[!?.,]?\\s+alfred";  // Start phrase regex pattern (matches "hey alfred", "hey! alfred", "hey, alfred", etc.)
    std::string stop_phrase = "stop[!?.,]?\\s+alfred";  // Stop phrase regex pattern (matches "stop alfred", "stop! alfred", "stop, alfred", etc.)
    bool verbose = false;  // Enable real-time chunk printing
    bool enable_vad = true;  // Enable VAD filtering (enabled by default)
    bool enable_noise_reduction = true;  // Enable noise reduction (enabled by default)
    int vad_mode = 2;  // VAD aggressiveness (0-3, WebRTC standard: 0=quality, 1=low bitrate, 2=aggressive, 3=very aggressive)
    float noise_reduction_amount = 0.5f;  // Noise reduction strength (0.0-1.0)
    bool show_help = false;
    bool valid = true;  // false if parsing failed
};

// Parse command line arguments and return configuration
Config parse_args(int argc, char* argv[]);

// Print usage information
void print_usage(const char* program_name);

