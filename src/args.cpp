#include "args.h"
#include <iostream>
#include <string>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "Options:\n"
              << "  -o, --output DIR    Output directory (default: output)\n"
              << "  -f, --filename NAME Custom filename (default: auto-generated with timestamp)\n"
              << "  -h, --help          Show this help message\n";
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
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            config.valid = false;
            return config;
        }
    }
    
    return config;
}

