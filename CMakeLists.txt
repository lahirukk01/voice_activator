cmake_minimum_required(VERSION 3.16)
project(M1Recorder)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Helpful for M1: Find Homebrew's SDL2
set(CMAKE_PREFIX_PATH "/opt/homebrew")
find_package(SDL2 REQUIRED)

# Add the MACOSX_BUNDLE flag to add_executable
add_executable(WhisperRecorder MACOSX_BUNDLE 
    src/main.cpp
    src/args.cpp
)

# Link your plist template
set_target_properties(WhisperRecorder PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
)

# Include directories
target_include_directories(WhisperRecorder PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Link libraries
target_link_libraries(WhisperRecorder PRIVATE ${SDL2_LIBRARIES})
