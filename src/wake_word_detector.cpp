#include "wake_word_detector.hpp"
#include "audio_capture.hpp"
#include "transcribe.hpp"
#include "audio_filter.hpp"
#include "wake_word_event.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <algorithm>
#include <vector>
#include <string>
#include <chrono>
#include <memory>

WakeWordDetector::WakeWordDetector()
    : dev_(0)
    , stop_requested_(false)
    , phrase_start_detected_(false)
    , initialized_(false)
    , running_(false)
    , event_channel_(nullptr)
{
}

WakeWordDetector::~WakeWordDetector() {
    cleanup();
}

bool WakeWordDetector::initialize(const Config& config, Channel<WakeWordEvent>& event_channel) {
    if (initialized_) {
        std::cerr << "WakeWordDetector already initialized" << std::endl;
        return false;
    }
    
    config_ = config;
    event_channel_ = &event_channel;
    
    // Create transcriber and audio filter
    transcriber_ = std::make_unique<WhisperTranscriber>();
    audio_filter_ = std::make_unique<AudioFilter>();
    
    // Initialize whisper asynchronously early
    transcriber_->init_async(config_.model_path, config_.verbose);
    
    // Initialize audio filter (VAD + noise reduction)
    const int sample_rate = 16000;
    if (config_.enable_vad) {
        audio_filter_->init_vad(sample_rate, config_.vad_mode);
    }
    if (config_.enable_noise_reduction) {
        audio_filter_->init_noise_reduction(sample_rate, config_.noise_reduction_amount);
    }
    set_audio_filter(audio_filter_.get());
    
    if (!init_audio_capture()) {
        std::cerr << "Failed to initialize audio system" << std::endl;
        return false;
    }
    
    initialized_ = true;
    return true;
}

bool WakeWordDetector::setup_audio_device() {
    // Open default capture device
    SDL_AudioSpec obtained;
    dev_ = open_audio_device(&obtained);
    
    if (dev_ == 0) {
        std::cerr << "Failed to open mic: " << SDL_GetError() << std::endl;
        return false;
    }

    // Print audio device info
    std::cout << "Audio device opened:" << std::endl;
    std::cout << "  Sample rate: " << obtained.freq << " Hz" << std::endl;
    std::cout << "  Format: " << (obtained.format == AUDIO_F32 ? "F32" : "Other") << std::endl;
    std::cout << "  Channels: " << static_cast<int>(obtained.channels) << std::endl;
    std::cout << "  Buffer size: " << obtained.samples << " samples" << std::endl;
    
    // Check if format matches what we need
    const int target_sample_rate = 16000;
    if (obtained.freq != target_sample_rate) {
        std::cout << "  Warning: Sample rate mismatch (expected " << target_sample_rate 
                  << " Hz, got " << obtained.freq << " Hz)" << std::endl;
        std::cout << "  Audio will be resampled to " << target_sample_rate << " Hz for transcription" << std::endl;
    }
    if (g_audio_format.is_stereo) {
        std::cout << "  Note: Converting stereo to mono" << std::endl;
    }
    
    std::cout << "VAD: " << (config_.enable_vad ? "enabled" : "disabled") << std::endl;
    std::cout << "Noise reduction: " << (config_.enable_noise_reduction ? "enabled" : "disabled") << std::endl;
    std::cout << "Recording... Press Enter to stop." << std::endl;
    
    start_audio_capture(dev_);
    
    return true;
}

std::vector<float> WakeWordDetector::resample_chunk(const std::vector<float>& chunk, 
                                                     int target_sample_rate) const {
    if (g_audio_format.sample_rate == target_sample_rate) {
        return chunk;  // No resampling needed
    }
    
    std::vector<float> resampled_chunk;
    size_t target_size = static_cast<size_t>(chunk.size() * target_sample_rate / g_audio_format.sample_rate);
    resampled_chunk.reserve(target_size);
    
    double ratio = static_cast<double>(g_audio_format.sample_rate) / target_sample_rate;
    for (size_t i = 0; i < target_size; i++) {
        double src_index = i * ratio;
        size_t idx0 = static_cast<size_t>(src_index);
        size_t idx1 = std::min(idx0 + 1, chunk.size() - 1);
        double frac = src_index - idx0;
        
        float sample = chunk[idx0] * (1.0f - frac) + chunk[idx1] * frac;
        resampled_chunk.push_back(sample);
    }
    
    return resampled_chunk;
}

void WakeWordDetector::handle_transcription(const std::string& transcription) {
    if (transcription.empty() || event_channel_ == nullptr) {
        return;
    }
    
    // Check for phrases
    int phrase_result = transcriber_->check_phrases(
        transcription, 
        config_.start_phrase, 
        config_.stop_phrase
    );
    
    // Handle phrase detection and send events
    if (phrase_result == 1 && !phrase_start_detected_.load()) {
        phrase_start_detected_.store(true);
        std::cout << "[START PHRASE DETECTED] " << transcription << std::endl;
        
        // Send START event to main thread
        event_channel_->send(WakeWordEvent(WakeWordEventType::START, transcription));
    } else if (phrase_result == -1 && phrase_start_detected_.load()) {
        std::cout << "[STOP PHRASE DETECTED] " << transcription << std::endl;
        phrase_start_detected_.store(false);
        
        // Send STOP event to main thread
        event_channel_->send(WakeWordEvent(WakeWordEventType::STOP, transcription));
    }

    // Always print transcriptions
    std::cout << "[TRANSCRIPTION] " << transcription << std::endl;
}

void WakeWordDetector::process_audio_chunks() {
    // Wait for whisper to be ready
    transcriber_->wait_for_ready();
    
    // Chunk processing variables - use actual sample rate for buffer calculations
    // But whisper needs 16kHz, so we'll resample chunks before transcription
    const int whisper_sample_rate = 16000;
    const size_t chunk_size_samples = static_cast<size_t>(config_.chunk_size_seconds * g_audio_format.sample_rate);
    const size_t overlap_samples = static_cast<size_t>(0.2 * g_audio_format.sample_rate);  // 0.2s overlap
    size_t last_processed_index = 0;
    
    while (!stop_requested_.load()) {
        // Wait for enough audio to be captured
        size_t current_size = g_audio_buffer.size();
        size_t required_size = last_processed_index + chunk_size_samples;
        
        if (current_size < required_size) {
            // Not enough audio yet, wait a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Extract chunk with overlap
        size_t chunk_start = (last_processed_index >= overlap_samples) 
            ? last_processed_index - overlap_samples 
            : 0;
        size_t chunk_end = last_processed_index + chunk_size_samples;
        chunk_end = std::min(chunk_end, current_size);
        
        if (chunk_end <= chunk_start) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Extract chunk
        std::vector<float> chunk(
            g_audio_buffer.begin() + chunk_start,
            g_audio_buffer.begin() + chunk_end
        );
        
        // Resample to 16kHz if needed
        chunk = resample_chunk(chunk, whisper_sample_rate);
        
        // Transcribe chunk (now at 16kHz)
        std::string transcription = transcriber_->transcribe_chunk(chunk);
        
        if (!transcription.empty()) {
            handle_transcription(transcription);
        } else if (config_.verbose) {
            // Debug: show when chunks are processed but return empty
            std::cout << "[DEBUG] Chunk processed but transcription empty (chunk size: " 
                      << chunk.size() << " samples)" << std::endl;
        }
        
        // Update last processed index (move forward by chunk size minus overlap)
        last_processed_index += (chunk_size_samples - overlap_samples);
    }
}

int WakeWordDetector::start() {
    if (!initialized_) {
        std::cerr << "WakeWordDetector not initialized. Call initialize() first." << std::endl;
        return 1;
    }
    
    if (running_) {
        std::cerr << "WakeWordDetector already running" << std::endl;
        return 1;
    }
    
    if (event_channel_ == nullptr) {
        std::cerr << "Event channel not set. Call initialize() with a valid channel." << std::endl;
        return 1;
    }
    
    // Setup audio device
    if (!setup_audio_device()) {
        return 1;
    }
    
    // Reset flags
    stop_requested_ = false;
    phrase_start_detected_ = false;
    running_ = true;
    
    // Create detector thread (runs process_audio_chunks)
    detector_thread_ = std::thread(&WakeWordDetector::process_audio_chunks, this);
    
    // Return immediately - caller will run event loop
    return 0;
}

void WakeWordDetector::run_event_loop() {
    if (!running_) {
        std::cerr << "Detector not running. Call start() first." << std::endl;
        return;
    }
    
    std::cout << "Waiting for wake word events... (Call stop() to exit)" << std::endl;
    
    while (!event_channel_->is_closed() && running_) {
        // Blocking receive - waits until an event is available or channel is closed
        WakeWordEvent event = event_channel_->receive();
        
        // If channel is closed and empty, receive returns default event
        if (event_channel_->is_closed()) {
            break;
        }
        
        // Handle event (for C++ use case, can add callbacks here if needed)
        if (event.type == WakeWordEventType::START) {
            std::cout << "\n[START] Event received! Transcription: " 
                      << event.transcription << std::endl;
        } else if (event.type == WakeWordEventType::STOP) {
            std::cout << "\n[STOP] Event received! Transcription: " 
                      << event.transcription << std::endl;
        }
    }
    
    // Event loop exited, join detector thread
    if (detector_thread_.has_value() && detector_thread_->joinable()) {
        detector_thread_->join();
        detector_thread_.reset();
    }
    
    running_ = false;
    std::cout << "Captured " << g_audio_buffer.size() << " samples." << std::endl;
}

void WakeWordDetector::stop() {
    if (!running_) {
        return;
    }
    
    // Signal stop (causes process_audio_chunks loop to exit)
    stop_requested_ = true;
    stop_audio_capture(dev_);
    
    // Close event channel (unblocks event loop)
    if (event_channel_ != nullptr) {
        event_channel_->close();
    }
    
    // Wait for detector thread to finish
    if (detector_thread_.has_value() && detector_thread_->joinable()) {
        detector_thread_->join();
        detector_thread_.reset();
    }
    
    running_ = false;
}

void WakeWordDetector::cleanup() {
    stop();
    
    if (dev_ != 0) {
        SDL_CloseAudioDevice(dev_);
        dev_ = 0;
    }
    
    cleanup_audio();
    
    // Cleanup transcriber
    if (transcriber_) {
        transcriber_->cleanup();
        transcriber_.reset();
    }
    
    // Cleanup audio filter
    if (audio_filter_) {
        audio_filter_->cleanup();
        audio_filter_.reset();
    }
    
    set_audio_filter(nullptr);
    initialized_ = false;
    event_channel_ = nullptr;
}

// Convenience function for backward compatibility
// Handles full lifecycle: initialization, event processing, and cleanup
int start_wake_word_detection(const Config& config) {
    // Create event channel
    Channel<WakeWordEvent> event_channel;
    
    // Create wake word detector
    WakeWordDetector detector;
    
    if (!detector.initialize(config, event_channel)) {
        return 1;
    }
    
    // Start detector (creates detector thread, returns immediately)
    if (detector.start() != 0) {
        return 1;
    }
    
    // Wait for user input in separate thread
    std::thread input_thread([&detector]() {
        std::cin.get();
        detector.stop();
    });
    
    // Run event loop in main thread (blocks until stop() is called)
    detector.run_event_loop();
    
    // Wait for input thread to finish
    if (input_thread.joinable()) {
        input_thread.join();
    }
    
    // Cleanup
    detector.cleanup();
    
    return 0;
}

