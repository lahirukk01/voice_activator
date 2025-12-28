cmake_minimum_required(VERSION 3.16)
project(M1Recorder)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Apple Silicon (M1/M1 Pro/M1 Max/M2) optimization
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|ARM64")
    # Set CPU flags for Apple Silicon
    # M1 Pro uses A14 architecture, -mcpu=apple-m1 works for all M1 variants
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=apple-m1" CACHE STRING "C compiler flags" FORCE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=apple-m1" CACHE STRING "C++ compiler flags" FORCE)
    
    # Alternative: use -march for broader compatibility
    # set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8.4-a+fp16+dotprod" CACHE STRING "C compiler flags" FORCE)
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8.8-a+fp16+dotprod" CACHE STRING "C++ compiler flags" FORCE)
    
    message(STATUS "Apple Silicon detected - using -mcpu=apple-m1 optimization")
endif()

# OpenMP configuration for Homebrew installation
if(APPLE)
    set(OpenMP_C_FLAGS "-Xclang -fopenmp" CACHE STRING "OpenMP C flags")
    set(OpenMP_C_LIB_NAMES "omp" CACHE STRING "OpenMP C library names")
    set(OpenMP_CXX_FLAGS "-Xclang -fopenmp" CACHE STRING "OpenMP CXX flags")
    set(OpenMP_CXX_LIB_NAMES "omp" CACHE STRING "OpenMP CXX library names")
    set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib" CACHE FILEPATH "OpenMP library path")
    set(OpenMP_C_INCLUDE_DIR "/opt/homebrew/opt/libomp/include" CACHE PATH "OpenMP C include directory")
    set(OpenMP_CXX_INCLUDE_DIR "/opt/homebrew/opt/libomp/include" CACHE PATH "OpenMP CXX include directory")
endif()

# Helpful for M1: Find Homebrew's SDL2
set(CMAKE_PREFIX_PATH "/opt/homebrew")
find_package(SDL2 REQUIRED)

# Add whisper.cpp as a subdirectory
# Use absolute path to whisper.cpp
set(WHISPER_CPP_DIR "/Users/lahirukk/SoftwareProjects/Python/whisper.cpp")
if(NOT EXISTS "${WHISPER_CPP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "whisper.cpp not found at ${WHISPER_CPP_DIR}. Please update WHISPER_CPP_DIR in CMakeLists.txt")
endif()

# Set whisper.cpp build options before adding subdirectory
# These can be overridden via command line: cmake .. -DGGML_OPENMP=OFF etc.
set(GGML_OPENMP ON CACHE BOOL "Enable OpenMP support")
set(GGML_METAL ON CACHE BOOL "Enable Metal support (Apple Silicon)")
set(WHISPER_COREML ON CACHE BOOL "Enable CoreML support (Apple Neural Engine)")

# Set ARM architecture for Apple Silicon to avoid CPU detection warnings
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|ARM64")
    # Explicitly unset GGML_CPU_ALL_VARIANTS if it was cached from a previous build
    # unset(GGML_CPU_ALL_VARIANTS CACHE)
    
    # Disable native CPU detection to avoid the warning
    # The -mcpu=apple-m1 flag we set earlier will be used for optimization
    set(GGML_NATIVE OFF CACHE BOOL "Disable native CPU detection" FORCE)
    
    # Set explicit ARM architecture for M1 Pro
    # armv8.5-a supports the features M1 Pro has (FP16, dot product)
    set(GGML_CPU_ARM_ARCH "armv8.5-a+fp16+dotprod" CACHE STRING "ARM architecture for CPU backend" FORCE)
    
    message(STATUS "Apple Silicon: Using explicit ARM architecture: ${GGML_CPU_ARM_ARCH}")
endif()

message(STATUS "Using whisper.cpp from: ${WHISPER_CPP_DIR}")
message(STATUS "Whisper.cpp options:")
message(STATUS "  GGML_OPENMP: ${GGML_OPENMP}")
message(STATUS "  GGML_METAL: ${GGML_METAL}")
message(STATUS "  WHISPER_COREML: ${WHISPER_COREML}")

add_subdirectory(${WHISPER_CPP_DIR} whisper_build)

# Find audio processing libraries (libfvad and libspecbleach)
# These libraries are not available via Homebrew, so they need to be built from source.
# Run ./install_audio_libs.sh to build and install them.

find_package(PkgConfig)

# Add common install paths to pkg-config search
if(APPLE)
    set(ENV{PKG_CONFIG_PATH} "/usr/local/lib/pkgconfig:/opt/homebrew/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()

# Try to find libfvad
set(HAVE_FVAD OFF)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBFVAD libfvad)
    if(LIBFVAD_FOUND)
        # Verify header file actually exists
        find_path(FVAD_HEADER fvad.h
            PATHS ${LIBFVAD_INCLUDE_DIRS} /usr/local/include /opt/homebrew/include
        )
        if(FVAD_HEADER)
            set(HAVE_FVAD ON)
            message(STATUS "Found libfvad: ${LIBFVAD_LIBRARIES}")
            message(STATUS "  Header at: ${FVAD_HEADER}")
        else()
            message(STATUS "libfvad pkg-config found but header not available, using fallback VAD")
            set(LIBFVAD_FOUND OFF)
        endif()
    else()
        message(STATUS "libfvad not found via pkg-config, using fallback VAD")
        message(STATUS "  To install: run ./install_audio_libs.sh")
    endif()
else()
    message(STATUS "pkg-config not found, using fallback VAD")
    message(STATUS "  Install pkg-config: brew install pkg-config")
    message(STATUS "  Then install libraries: run ./install_audio_libs.sh")
endif()

# Try to find libspecbleach
set(HAVE_SPECBLEACH OFF)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBSPECBLEACH libspecbleach)
    if(LIBSPECBLEACH_FOUND)
        # Verify header file actually exists
        find_path(SPECBLEACH_HEADER specbleach_denoiser.h
            PATHS ${LIBSPECBLEACH_INCLUDE_DIRS} /usr/local/include /opt/homebrew/include
        )
        if(SPECBLEACH_HEADER)
            set(HAVE_SPECBLEACH ON)
            message(STATUS "Found libspecbleach: ${LIBSPECBLEACH_LIBRARIES}")
            message(STATUS "  Header at: ${SPECBLEACH_HEADER}")
        else()
            message(STATUS "libspecbleach pkg-config found but header not available, using fallback noise reduction")
            set(LIBSPECBLEACH_FOUND OFF)
        endif()
    else()
        message(STATUS "libspecbleach not found via pkg-config, using fallback noise reduction")
        message(STATUS "  To install: run ./install_audio_libs.sh")
    endif()
else()
    message(STATUS "pkg-config not found, using fallback noise reduction")
endif()

# Add the MACOSX_BUNDLE flag to add_executable
add_executable(WhisperRecorder MACOSX_BUNDLE 
    src/main.cpp
    src/args.cpp
    src/transcribe.cpp
    src/audio_filter.cpp
    src/audio_capture.cpp
    src/wake_word_detector.cpp
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
    ${LIBFVAD_INCLUDE_DIRS}
    ${LIBSPECBLEACH_INCLUDE_DIRS}
)

# Define preprocessor flags for library availability
if(HAVE_FVAD)
    target_compile_definitions(WhisperRecorder PRIVATE HAVE_FVAD)
endif()

if(HAVE_SPECBLEACH)
    target_compile_definitions(WhisperRecorder PRIVATE HAVE_SPECBLEACH)
endif()

# Link libraries
target_link_libraries(WhisperRecorder PRIVATE 
    ${SDL2_LIBRARIES}
    whisper
    ${LIBFVAD_LIBRARIES}
    ${LIBSPECBLEACH_LIBRARIES}
)

# Add library directories if found
if(LIBFVAD_FOUND)
    target_link_directories(WhisperRecorder PRIVATE ${LIBFVAD_LIBRARY_DIRS})
endif()

if(LIBSPECBLEACH_FOUND)
    target_link_directories(WhisperRecorder PRIVATE ${LIBSPECBLEACH_LIBRARY_DIRS})
endif()
