#pragma once

#include <string>
#include <vector>

// Transcribe audio using whisper.cpp
// audio_data: 16kHz mono float32 PCM audio samples
// model_path: Path to the whisper model file (e.g., "models/ggml-base.en.bin")
// Returns: Transcribed text, or empty string on error
std::string transcribe_audio(const std::vector<float>& audio_data, const std::string& model_path);

