#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "args.hpp"
#include "wake_word_event.hpp"
#include "wake_word_detector.hpp"
#include "channel.hpp"
#include <thread>
#include <atomic>
#include <stdexcept>
#include <chrono>

namespace py = pybind11;

// Python wrapper class that uses event channel pattern to avoid GIL issues
class WakeWordDetectorPython {
public:
    WakeWordDetectorPython() : detector_(), event_channel_(), running_(false) {}
    
    ~WakeWordDetectorPython() {
        cleanup();
    }
    
    void initialize(const Config& config) {
        if (!detector_.initialize(config, event_channel_)) {
            throw std::runtime_error("Failed to initialize detector");
        }
    }
    
    void register_start_callback(py::object callback) {
        start_callback_ = callback;
    }
    
    void register_stop_callback(py::object callback) {
        stop_callback_ = callback;
    }
    
    void start() {
        if (running_) {
            throw std::runtime_error("Detector already running");
        }
        
        running_ = true;
        
        // Start detector (creates detector thread, returns immediately)
        if (detector_.start() != 0) {
            running_ = false;
            throw std::runtime_error("Failed to start detector");
        }
        
        // Return immediately - Python will run event loop separately
        // This allows Python to do heavy lifting without blocking
    }
    
    // Receive event with timeout (non-blocking for Python)
    // Returns event if received, None if timeout or channel closed
    py::object receive_event(double timeout = 0.1) {
        if (!running_ || event_channel_.is_closed()) {
            return py::none();
        }
        
        WakeWordEvent event;
        auto timeout_duration = std::chrono::duration<double>(timeout);
        bool received = event_channel_.receive_timeout(event, timeout_duration);
        
        if (!received || event_channel_.is_closed()) {
            return py::none();
        }
        
        // Return event as Python object
        return py::cast(event);
    }
    
    void stop() {
        if (!running_) {
            return;
        }
        
        // Stop detector (sets flag, closes channel, joins detector thread)
        detector_.stop();
        
        running_ = false;
    }
    
    void cleanup() {
        stop();
        detector_.cleanup();
    }
    
    bool is_running() const {
        return running_;
    }
    
    WakeWordDetector detector_;
    Channel<WakeWordEvent> event_channel_;
    py::object start_callback_;
    py::object stop_callback_;
    std::atomic<bool> running_;
};

PYBIND11_MODULE(voice_recorder, m) {
    m.doc() = "Voice recorder with wake word detection";
    
    // Expose WakeWordEventType enum
    py::enum_<WakeWordEventType>(m, "WakeWordEventType")
        .value("START", WakeWordEventType::START)
        .value("STOP", WakeWordEventType::STOP);
    
    // Expose WakeWordEvent struct
    py::class_<WakeWordEvent>(m, "WakeWordEvent")
        .def(py::init<>())
        .def(py::init<WakeWordEventType, const std::string&>())
        .def_readwrite("type", &WakeWordEvent::type)
        .def_readwrite("transcription", &WakeWordEvent::transcription);
    
    // Expose Config struct
    py::class_<Config>(m, "Config")
        .def(py::init<>())
        .def_readwrite("output_dir", &Config::output_dir)
        .def_readwrite("custom_filename", &Config::custom_filename)
        .def_readwrite("model_path", &Config::model_path)
        .def_readwrite("chunk_size_seconds", &Config::chunk_size_seconds)
        .def_readwrite("start_phrase", &Config::start_phrase)
        .def_readwrite("stop_phrase", &Config::stop_phrase)
        .def_readwrite("verbose", &Config::verbose)
        .def_readwrite("enable_vad", &Config::enable_vad)
        .def_readwrite("enable_noise_reduction", &Config::enable_noise_reduction)
        .def_readwrite("vad_mode", &Config::vad_mode)
        .def_readwrite("noise_reduction_amount", &Config::noise_reduction_amount);
    
    // Expose WakeWordDetectorPython wrapper class
    py::class_<WakeWordDetectorPython>(m, "WakeWordDetector")
        .def(py::init<>())
        .def("initialize", &WakeWordDetectorPython::initialize, "Initialize the detector", py::arg("config"))
        .def("register_start_callback", &WakeWordDetectorPython::register_start_callback, 
             "Register callback for START events", py::arg("callback"))
        .def("register_stop_callback", &WakeWordDetectorPython::register_stop_callback,
             "Register callback for STOP events", py::arg("callback"))
        .def("start", &WakeWordDetectorPython::start, "Start detection (non-blocking, returns immediately)")
        .def("receive_event", &WakeWordDetectorPython::receive_event, 
             "Receive event with timeout (non-blocking)", py::arg("timeout") = 0.1)
        .def("stop", &WakeWordDetectorPython::stop, "Stop detection")
        .def("cleanup", &WakeWordDetectorPython::cleanup, "Cleanup resources")
        .def("is_running", &WakeWordDetectorPython::is_running, "Check if detector is running");
}

