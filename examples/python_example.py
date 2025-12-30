#!/usr/bin/env python3
"""
Example usage of voice_recorder Python module.

This demonstrates:
- Creating a wake word detector
- Registering callbacks for START/STOP events
- Starting detection (non-blocking)
- Running event loop in Python (allows heavy lifting)
- Stopping from signal handler
"""

import sys
import os

# Add build directory to Python path so we can import voice_recorder
build_dir = os.path.join(os.path.dirname(__file__), '..', 'build')
build_dir = os.path.abspath(build_dir)
if build_dir not in sys.path:
    sys.path.insert(0, build_dir)

import voice_recorder
import signal
import time

# Global detector instance for signal handler
detector = None

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    print("\nReceived interrupt signal, stopping detector...")
    if detector:
        detector.stop()
        detector.cleanup()
    sys.exit(0)

def on_start(event):
    """Callback for START events - lightweight, can queue heavy work"""
    print(f"\n[START] Wake word detected! Transcription: {event.transcription}")
    print("  -> You can start your assistant/recording here")
    # Do heavy lifting in separate thread or async task

def on_stop(event):
    """Callback for STOP events - lightweight, can queue heavy work"""
    print(f"\n[STOP] Stop phrase detected! Transcription: {event.transcription}")
    print("  -> You can stop your assistant/recording here")
    # Do heavy lifting in separate thread or async task

def main():
    global detector
    
    # Register signal handler for graceful shutdown
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    # Create config
    config = voice_recorder.Config()
    config.model_path = "/Users/lahirukk/SoftwareProjects/Python/whisper.cpp/models/ggml-base.en.bin"
    config.start_phrase = "hey[!?.,]?\\s+alfred"
    config.stop_phrase = "stop[!?.,]?\\s+alfred"
    config.verbose = True
    config.enable_vad = False  # Disable VAD for debugging
    config.enable_noise_reduction = False  # Disable noise reduction for debugging
    
    # Create detector
    detector = voice_recorder.WakeWordDetector()
    
    # Initialize (creates event channel internally)
    try:
        detector.initialize(config)
    except Exception as e:
        print(f"Failed to initialize detector: {e}")
        return 1
    
    # Register callbacks
    detector.register_start_callback(on_start)
    detector.register_stop_callback(on_stop)
    
    # Start detection (non-blocking, returns immediately)
    print("Starting wake word detection...")
    print("Say 'hey alfred' to trigger START, 'stop alfred' to trigger STOP")
    print("Press Ctrl+C to stop")
    
    try:
        detector.start()  # Non-blocking, returns immediately
        
        # Python runs its own event loop - can do heavy lifting between events
        while detector.is_running():
            # Receive event with timeout (non-blocking)
            event = detector.receive_event(timeout=0.1)
            
            if event is not None:
                # Call appropriate callback (lightweight - can queue heavy work)
                if event.type == voice_recorder.WakeWordEventType.START:
                    on_start(event)
                elif event.type == voice_recorder.WakeWordEventType.STOP:
                    on_stop(event)
            
            # Python can do heavy lifting here between event checks
            # This is where you'd do CPU-intensive work, ML inference, file I/O, etc.
            # The 100ms timeout gives you ~90ms for work between checks
            # Example: process_queue(), run_inference(), etc.
            
    except KeyboardInterrupt:
        pass
    finally:
        print("\nStopping detector...")
        detector.stop()
        detector.cleanup()
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
