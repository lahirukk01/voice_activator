#pragma once

#include "audio_utils.hpp"
#include <string>

// Forward declaration
class AudioFilter;

class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual AudioQueue<float>& get_audio_queue() = 0;
    virtual const AudioFormat& get_format() const = 0;
    virtual void set_audio_filter(AudioFilter* filter) = 0;
    virtual void set_verbose(bool verbose) = 0;
};
