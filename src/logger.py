import json
import threading

from src.events import Event


class EventLogger:
    def __init__(self, filepath: str):
        self.file = open(filepath, "a")
        self.lock = threading.Lock()

    def __call__(self, event: Event):
        entry = {
            "event_type": event.type,
            **event.data,
        }

        line = json.dumps(entry) + "\n"

        with self.lock:
            self.file.write(line)
            self.file.flush()

    def close(self):
        with self.lock:
            self.file.close()
