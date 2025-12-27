#pragma once

#include <string>

struct Config {
    std::string output_dir = "output";
    std::string custom_filename = "";  // Empty means use auto-generated timestamp
    std::string model_path = "/Users/lahirukk/SoftwareProjects/Python/whisper.cpp/models/ggml-base.en.bin";  // Default to base.en model
    bool show_help = false;
    bool valid = true;  // false if parsing failed
};

// Parse command line arguments and return configuration
Config parse_args(int argc, char* argv[]);

// Print usage information
void print_usage(const char* program_name);

