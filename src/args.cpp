#include "args.h"
#include <iostream>
#include <string>
#include <cstdlib>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "Options:\n"
              << "  -o, --output DIR       Output directory (default: output)\n"
              << "  -f, --filename NAME    Custom filename (default: auto-generated with timestamp)\n"
              << "  -m, --model PATH       Path to whisper model file (default: ../Python/whisper.cpp/models/ggml-base.en.bin)\n"
              << "  --chunk-size SECONDS   Chunk size for transcription in seconds (default: 3.0)\n"
              << "  --start-phrase REGEX   Start phrase regex pattern (default: \"hey alfred\")\n"
              << "  --stop-phrase REGEX    Stop phrase regex pattern (default: \"stop alfred\")\n"
              << "  --verbose              Enable real-time chunk printing\n"
              << "  -h, --help             Show this help message\n";
}

Config parse_args(int argc, char* argv[]) {
    Config config;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            config.show_help = true;
            return config;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                config.output_dir = argv[++i];
            } else {
                std::cerr << "Error: --output requires a directory path\n";
                config.valid = false;
                return config;
            }
        } else if (arg == "-f" || arg == "--filename") {
            if (i + 1 < argc) {
                config.custom_filename = argv[++i];
            } else {
                std::cerr << "Error: --filename requires a filename\n";
                config.valid = false;
                return config;
            }
        } else if (arg == "-m" || arg == "--model") {
            if (i + 1 < argc) {
                config.model_path = argv[++i];
            } else {
                std::cerr << "Error: --model requires a model file path\n";
                config.valid = false;
                return config;
            }
        } else if (arg == "--chunk-size") {
            if (i + 1 < argc) {
                config.chunk_size_seconds = std::stod(argv[++i]);
                if (config.chunk_size_seconds <= 0) {
                    std::cerr << "Error: --chunk-size must be greater than 0\n";
                    config.valid = false;
                    return config;
                }
            } else {
                std::cerr << "Error: --chunk-size requires a number\n";
                config.valid = false;
                return config;
            }
        } else if (arg == "--start-phrase") {
            if (i + 1 < argc) {
                config.start_phrase = argv[++i];
            } else {
                std::cerr << "Error: --start-phrase requires a regex pattern\n";
                config.valid = false;
                return config;
            }
        } else if (arg == "--stop-phrase") {
            if (i + 1 < argc) {
                config.stop_phrase = argv[++i];
            } else {
                std::cerr << "Error: --stop-phrase requires a regex pattern\n";
                config.valid = false;
                return config;
            }
        } else if (arg == "--verbose") {
            config.verbose = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            config.valid = false;
            return config;
        }
    }
    
    return config;
}

