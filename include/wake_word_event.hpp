#pragma once

#include <string>

// Wake word event types
enum class WakeWordEventType {
    START,
    STOP
};

// Wake word event structure
struct WakeWordEvent {
    WakeWordEventType type;
    std::string transcription;  // The transcription that triggered the event
    
    WakeWordEvent() : type(WakeWordEventType::START) {}
    WakeWordEvent(WakeWordEventType t, const std::string& trans = "") 
        : type(t), transcription(trans) {}
};

