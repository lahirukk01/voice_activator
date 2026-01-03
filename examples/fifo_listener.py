#!/usr/bin/env python3
"""
Python listener for wake word events via named pipe (FIFO).

This script:
- Opens a FIFO for reading (blocking)
- Listens for JSON events: {"type":"START"} or {"type":"STOP"}
- Handles events as they arrive
- Keeps FIFO open for the entire session
"""

import os
import json
import sys
import signal

class FifoListener:
    def __init__(self, fifo_path):
        self.fifo_path = fifo_path
        self.fifo = None
        
        # Create FIFO if it doesn't exist
        if not os.path.exists(fifo_path):
            os.mkfifo(fifo_path)
            print(f"Created FIFO: {fifo_path}")
    
    def open(self):
        """Open FIFO for reading (blocks until writer opens it)"""
        print(f"Opening FIFO: {self.fifo_path}")
        print("Waiting for C++ detector to connect...")
        self.fifo = open(self.fifo_path, "r")
        print("FIFO opened, listening for events...")
    
    def listen(self):
        """Listen for events in a loop (blocking readline)"""
        if not self.fifo:
            print("Error: FIFO not open", file=sys.stderr)
            return
        
        while True:
            # Blocking read - waits until a line is available
            line = self.fifo.readline()
            
            if not line:  # EOF - writer closed the pipe
                print("Writer closed FIFO, exiting...")
                break
            
            # Parse JSON event
            try:
                event = json.loads(line.strip())
                self.handle_event(event)
            except json.JSONDecodeError as e:
                print(f"Invalid JSON: {line.strip()}", file=sys.stderr)
    
    def handle_event(self, event):
        """Handle received event"""
        event_type = event.get("type")
        
        if event_type == "START":
            print("\n[START] Wake word detected!")
            print("  -> You can start your assistant/recording here")
            # Do heavy lifting here or in separate thread
        elif event_type == "STOP":
            print("\n[STOP] Stop phrase detected!")
            print("  -> You can stop your assistant/recording here")
            # Do heavy lifting here or in separate thread
        else:
            print(f"Unknown event type: {event_type}")
    
    def close(self):
        """Close FIFO"""
        if self.fifo:
            self.fifo.close()
            self.fifo = None
            print("FIFO closed")

def main():
    # Default FIFO path (can be overridden via command line)
    fifo_path = "/tmp/wake_word_events"
    
    if len(sys.argv) > 1:
        fifo_path = sys.argv[1]
    
    listener = FifoListener(fifo_path)
    
    # Signal handler for graceful shutdown
    def signal_handler(sig, frame):
        print("\nReceived interrupt signal, closing FIFO...")
        listener.close()
        sys.exit(0)
    
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    try:
        # Open FIFO (blocks until C++ writer opens it)
        listener.open()
        
        # Listen for events (blocking)
        listener.listen()
        
    except KeyboardInterrupt:
        pass
    finally:
        listener.close()
    
    return 0

if __name__ == "__main__":
    sys.exit(main())

