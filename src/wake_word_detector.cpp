#include "wake_word_detector.hpp"
#include "wake_word_event.hpp"
#include "fifo_writer.hpp"
#include <iostream>
#include <iostream>
#include <thread>
#include <atomic>
#include <algorithm>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <stdexcept>

WakeWordDetector::WakeWordDetector(
    IAudioCapture& audio_capture,
    ITranscriber& transcriber,
    double chunk_size_seconds,
    const std::string& start_phrase,
    const std::string& stop_phrase,
    bool verbose,
    Channel<WakeWordEvent>& event_channel
)
    : audio_capture_(audio_capture)
    , transcriber_(transcriber)
    , chunk_size_seconds_(chunk_size_seconds)
    , start_phrase_(start_phrase)
    , stop_phrase_(stop_phrase)
    , verbose_(verbose)
    , stop_requested_(false)
    , phrase_start_detected_(false)
    , running_(false)
    , event_channel_(event_channel)
{
    // Dependencies are injected and initialized by the caller (Application)
}

WakeWordDetector::~WakeWordDetector() {
    cleanup();
}

std::vector<float> WakeWordDetector::resample_chunk(const std::vector<float>& chunk, 
                                                     int target_sample_rate) const {
    const auto& format = audio_capture_.get_format();
    if (format.sample_rate == target_sample_rate) {
        return chunk;  // No resampling needed
    }
    
    std::vector<float> resampled_chunk;
    size_t target_size = static_cast<size_t>(chunk.size() * target_sample_rate / format.sample_rate);
    resampled_chunk.reserve(target_size);
    
    double ratio = static_cast<double>(format.sample_rate) / target_sample_rate;
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
    if (transcription.empty()) {
        return;
    }
    
    // Check for phrases
    int phrase_result = transcriber_.check_phrases(
        transcription, 
        start_phrase_, 
        stop_phrase_
    );
    
    // Handle phrase detection and send events
    if (phrase_result == 1 && !phrase_start_detected_.load()) {
        phrase_start_detected_.store(true);
        std::cout << "[START PHRASE DETECTED] " << transcription << std::endl;
        
        // Send START event to main thread
        event_channel_.send(WakeWordEvent(WakeWordEventType::START, transcription));
    } else if (phrase_result == -1 && phrase_start_detected_.load()) {
        std::cout << "[STOP PHRASE DETECTED] " << transcription << std::endl;
        phrase_start_detected_.store(false);
        
        // Send STOP event to main thread
        event_channel_.send(WakeWordEvent(WakeWordEventType::STOP, transcription));
    }

    // Always print transcriptions
    std::cout << "[TRANSCRIPTION] " << transcription << std::endl;
}

void WakeWordDetector::process_audio_chunks() {
    // Wait for whisper to be ready
    transcriber_.wait_for_ready();
    
    const int whisper_sample_rate = 16000;
    
    // We get pre-processed chunks from the queue
    // The queue yields vectors of float
    auto& queue = audio_capture_.get_audio_queue();
    
    // Current accumulated processing buffer
    std::vector<float> processing_buffer;
    
    // Calculate required samples for the configured chunk window
    // Note: This relies on the capture sample rate.
    // If capture sample rate is not 16k, we need to account for that.
    const int capture_rate = audio_capture_.get_format().sample_rate;
    const size_t target_chunk_samples = static_cast<size_t>(chunk_size_seconds_ * capture_rate);
    const size_t overlap_samples = static_cast<size_t>(0.2 * capture_rate); 
    
    if (verbose_) {
        std::cout << "[DEBUG] Audio processor started. Target chunk: " << target_chunk_samples << " samples." << std::endl;
    }

    while (!stop_requested_.load()) {
        // Wait for data (up to 100ms to allow checking stop_requested)
        if (!queue.wait_for_data(std::chrono::milliseconds(100))) {
            continue;
        }
        
        // Drain queue into processing buffer
        std::vector<float> data_chunk;
        while (queue.try_pop(data_chunk)) {
            processing_buffer.insert(processing_buffer.end(), data_chunk.begin(), data_chunk.end());
        }
        
        // Process as many full chunks as possible
        size_t cursor = 0;
        
        // While we have enough data (cursor + target <= size)
        while (processing_buffer.size() >= cursor + target_chunk_samples) {
            
            // Extract window
            std::vector<float> window(
                processing_buffer.begin() + cursor, 
                processing_buffer.begin() + cursor + target_chunk_samples
            );
            
            // Resample
            window = resample_chunk(window, whisper_sample_rate);
            
            // Transcribe
            std::string transcription = transcriber_.transcribe_chunk(window);
            if (!transcription.empty()) {
                handle_transcription(transcription);
            }
            
            // Move cursor forward by stride (window - overlap)
            size_t stride = target_chunk_samples - overlap_samples;
            cursor += stride;
        }
        
        // Keep the remaining overlap/unused buffer for next iteration
        if (cursor > 0) {
            if (cursor < processing_buffer.size()) {
                 std::vector<float> remaining(processing_buffer.begin() + cursor, processing_buffer.end());
                 processing_buffer = std::move(remaining);
            } else {
                 processing_buffer.clear();
            }
        }
    }
}

int WakeWordDetector::start() {
    if (running_) {
        std::cerr << "WakeWordDetector already running" << std::endl;
        return 1;
    }
    
    // Audio device is already opened by constructor
    
    // Print info
    const auto& fmt = audio_capture_.get_format();
    std::cout << "Audio Capture: " << fmt.sample_rate << " Hz, " << fmt.channels << " channels" << std::endl;
    
    // Reset flags
    stop_requested_ = false;
    phrase_start_detected_ = false;
    audio_capture_.get_audio_queue().clear();
    
    // Start capture
    audio_capture_.start();
    
    // Start chunk processing thread
    running_ = true;
    chunk_processor_thread_ = std::thread(&WakeWordDetector::process_audio_chunks, this);

    std::cout << "Recording... Press Enter to stop." << std::endl;

    // Wait for user input
    std::cin.get();
    
    // Stop processing
    stop();
    
    return 0;
}

void WakeWordDetector::stop() {
    if (!running_) {
        return;
    }
    
    stop_requested_ = true;
    audio_capture_.stop();
    
    if (chunk_processor_thread_.joinable()) {
        chunk_processor_thread_.join();
    }
    
    running_ = false;
}

void WakeWordDetector::cleanup() {
    stop();
}
    


