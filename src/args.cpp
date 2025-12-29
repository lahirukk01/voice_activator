#include "args.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "Options:\n"
              << "  -o, --output DIR       Output directory (default: output)\n"
              << "  -f, --filename NAME    Custom filename (default: auto-generated with timestamp)\n"
              << "  -m, --model PATH       Path to whisper model file (default: /Users/lahirukk/SoftwareProjects/Python/whisper.cpp/models/ggml-base.en.bin)\n"
              << "  --chunk-size SECONDS   Chunk size for transcription in seconds (default: 3.0)\n"
              << "  --start-phrase REGEX     Start phrase regex pattern (default: \"hey[!?.]?\\s+alfred\")\n"
              << "  --stop-phrase REGEX     Stop phrase regex pattern (default: \"stop[!?.]?\\s+alfred\")\n"
              << "  --verbose              Enable real-time chunk printing\n"
              << "  --no-vad               Disable VAD filtering\n"
              << "  --no-noise-reduction   Disable noise reduction\n"
              << "  --vad-mode MODE        Set VAD mode (0-3, default: 2)\n"
              << "                         0=quality, 1=low bitrate, 2=aggressive, 3=very aggressive\n"
              << "  --noise-reduction-amount FLOAT  Set noise reduction strength (0.0-1.0, default: 0.5)\n"
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
        } else if (arg == "--no-vad") {
            config.enable_vad = false;
        } else if (arg == "--no-noise-reduction") {
            config.enable_noise_reduction = false;
        } else if (arg == "--vad-mode") {
            if (i + 1 < argc) {
                int mode = std::stoi(argv[++i]);
                if (mode < 0 || mode > 3) {
                    std::cerr << "Error: --vad-mode must be between 0 and 3\n";
                    config.valid = false;
                    return config;
                }
                config.vad_mode = mode;
            } else {
                std::cerr << "Error: --vad-mode requires a number (0-3)\n";
                config.valid = false;
                return config;
            }
        } else if (arg == "--noise-reduction-amount") {
            if (i + 1 < argc) {
                float amount = std::stof(argv[++i]);
                if (amount < 0.0f || amount > 1.0f) {
                    std::cerr << "Error: --noise-reduction-amount must be between 0.0 and 1.0\n";
                    config.valid = false;
                    return config;
                }
                config.noise_reduction_amount = amount;
            } else {
                std::cerr << "Error: --noise-reduction-amount requires a number (0.0-1.0)\n";
                config.valid = false;
                return config;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            config.valid = false;
            return config;
        }
    }
    
    return config;
}

