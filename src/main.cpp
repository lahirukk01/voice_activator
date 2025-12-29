#include <iostream>
#include "args.hpp"
#include "wake_word_detector.hpp"

int main(int argc, char* argv[]) {
    // Parse command line arguments
    Config config = parse_args(argc, argv);
    
    if (config.show_help) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (!config.valid) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Start wake word detection (handles initialization, event processing, and cleanup)
    return start_wake_word_detection(config);
}
