#pragma once

#include <string>
#include <vector>

class ITranscriber {
public:
    virtual ~ITranscriber() = default;

    virtual void wait_for_ready() = 0;
    virtual std::string transcribe_chunk(const std::vector<float>& audio_data) = 0;
    virtual int check_phrases(const std::string& text, const std::string& start_phrase, const std::string& stop_phrase) = 0;
    
    // Note: init_async is not part of the interface used by WakeWordDetector,
    // as initialization is handled by the Application/Composition Root.
};
