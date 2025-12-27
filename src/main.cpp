#include <SDL.h>
#include <vector>
#include <iostream>
#include <atomic>
#include <filesystem> // For creating the directory
#include <chrono>
#include <sstream>
#include <iomanip>
#include <string>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include "args.h"
#include "transcribe.h"

// Buffer to store 16kHz mono float32 audio
std::vector<float> g_audio_buffer;
std::atomic<bool> g_is_recording(false);

void save_to_wav(const std::string& filename, const std::vector<float>& buffer, const std::string& output_dir = "output") {
    // 1. Ensure the output directory exists
    std::filesystem::create_directories(output_dir);
    std::string path = output_dir + "/" + filename;

    // 2. Define the format: 1 channel, 16000 Hz, 32-bit Float
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT; // Matches our Whisper buffer
    format.channels = 1;
    format.sampleRate = 16000;
    format.bitsPerSample = 32;

    drwav wav;
    if (drwav_init_file_write(&wav, path.c_str(), &format, nullptr)) {
        drwav_write_pcm_frames(&wav, buffer.size(), buffer.data());
        drwav_uninit(&wav);
        std::cout << "Saved to " << path << std::endl;
    } else {
        std::cerr << "Failed to open file for writing!" << std::endl;
    }
}

// Callback: Runs on a background thread managed by macOS CoreAudio
void audio_callback(void* userdata, Uint8* stream, int len) {
    if (!g_is_recording) return;

    // SDL provides bytes; cast to float for Whisper compatibility
    float* samples = reinterpret_cast<float*>(stream);
    size_t sample_count = len / sizeof(float);

    g_audio_buffer.insert(g_audio_buffer.end(), samples, samples + sample_count);
}

std::string create_filename() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "recording_" << std::put_time(std::localtime(&time_t), "%Y-%m-%d_%H-%M-%S") << ".wav";
    return ss.str();
}

SDL_AudioDeviceID open_audio_device() {
    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    desired.freq = 16000;       // Target: 16kHz
    desired.format = AUDIO_F32; // Whisper wants 32-bit floats
    desired.channels = 1;       // Mono
    desired.samples = 4096;
    desired.callback = audio_callback;

    // Open default capture device (iscapture = 1)
    return SDL_OpenAudioDevice(nullptr, 1, &desired, &obtained, 0);
}

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
    
    if (SDL_Init(SDL_INIT_AUDIO) < 0) return -1;

    // Open default capture device (iscapture = 1)
    SDL_AudioDeviceID dev = open_audio_device();
    
    if (dev == 0) {
        std::cerr << "Failed to open mic: " << SDL_GetError() << std::endl;
        return 1;
    }

    std::cout << "Recording... Press Enter to stop." << std::endl;
    g_is_recording = true;
    SDL_PauseAudioDevice(dev, 0); // Start the stream

    std::cin.get(); // Wait for user

    SDL_PauseAudioDevice(dev, 1); // Stop
    g_is_recording = false;
    
    std::cout << "Captured " << g_audio_buffer.size() << " samples." << std::endl;

    SDL_CloseAudioDevice(dev);
    SDL_Quit();

    std::string filename = config.custom_filename.empty() ? create_filename() : config.custom_filename;
    save_to_wav(filename, g_audio_buffer, config.output_dir);

    // Transcribe audio using the model (defaults to base.en if not specified)
    std::cout << "\nStarting transcription..." << std::endl;
    std::string transcription = transcribe_audio(g_audio_buffer, config.model_path);
    
    if (!transcription.empty()) {
        std::cout << "\n=== Transcription ===" << std::endl;
        std::cout << transcription << std::endl;
        std::cout << "=====================" << std::endl;
    } else {
        std::cerr << "Transcription failed or produced no output." << std::endl;
    }

    return 0;
}
