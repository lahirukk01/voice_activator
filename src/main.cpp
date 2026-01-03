#include <iostream>
#include "args.hpp"
#include "application.hpp"
#include <stdexcept>

int main(int argc, char** argv) {
    // Parse command line arguments
    Config config = parse_args(argc, argv);
    
    if (!config.valid) {
        // print_usage(argv[0]); // This line was removed in the requested change
        return 1;
    }
    
    if (config.show_help) {
        print_usage(argv[0]);
        return 0;
    }
    
    // Start application (handles initialization, event processing, and cleanup)
    try {
        Application::run(config);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
