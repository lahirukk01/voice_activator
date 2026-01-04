#include "application.hpp"
#include "wake_word_detector.hpp"
#include "fifo_writer.hpp"
#include "wake_word_event.hpp"
#include "channel.hpp"
#include "audio_capture.hpp"
#include "transcribe.hpp"
#include "audio_filter.hpp"
#include <iostream>
#include <memory>
#include <thread>

void Application::run(const Config& config) {
    Channel<WakeWordEvent> event_channel;
    
    // --- Composition Root ---
    
    // 1. Create Components
    auto transcriber = std::make_unique<WhisperTranscriber>();
    auto audio_filter = std::make_unique<AudioFilter>();
    auto audio_capture = std::make_unique<AudioCapture>(16000);
    auto fifo_writer = std::make_unique<FifoWriter>(config.fifo_path);

    // 2. Configure Transcriber
    transcriber->init_async(config.model_path, config.verbose);

    // 3. Configure AudioFilter
    const int sample_rate = 16000;
    if (config.vad_mode.has_value()) {
        audio_filter->init_vad(sample_rate, config.vad_mode.value());
    }
    if (config.noise_reduction_amount.has_value()) {
        audio_filter->init_noise_reduction(sample_rate, config.noise_reduction_amount.value());
    }

    // 4. Wire Filter to Capture
    audio_capture->set_audio_filter(audio_filter.get());
    audio_capture->set_verbose(config.verbose);

    // 5. Create Detector with Injected Dependencies
    auto detector = std::make_unique<WakeWordDetector>(
        *audio_capture,
        *transcriber,
        config.chunk_size_seconds,
        config.start_phrase,
        config.stop_phrase,
        config.verbose,
        event_channel
    );
    
    // --- End Composition Root ---
  
    std::thread detector_thread([&detector, &event_channel]() {
        detector->start(); 
        event_channel.close();
    });
    
    std::cout << "Waiting for wake word events... (Press Enter to stop)" << std::endl;
    
    while (!event_channel.is_closed()) {
        WakeWordEvent event = event_channel.receive();
        
        if (event_channel.is_closed()) {
            break;
        }
        
        if (event.type == WakeWordEventType::START) {
            std::cout << "\n[MAIN THREAD] START event received! Transcription: " 
                      << event.transcription << std::endl;
            fifo_writer->send_event(WakeWordEventType::START);
        } else if (event.type == WakeWordEventType::STOP) {
            std::cout << "\n[MAIN THREAD] STOP event received! Transcription: " 
                      << event.transcription << std::endl;
            fifo_writer->send_event(WakeWordEventType::STOP);
        }
    }
    
    if (detector_thread.joinable()) {
        detector_thread.join();
    }
}
