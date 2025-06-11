from dataclasses import dataclass
from collections import defaultdict
import threading
import queue

from typing import Callable, DefaultDict


@dataclass
class Event:
    type: str
    data: dict


class EventBus:
    def __init__(self):
        self.subscribers: DefaultDict[
                str, list[Callable[[Event], None]]
            ] = defaultdict(list)
        self.queue = queue.Queue()
        self.running = True
        self.worker = threading.Thread(target=self._run)
        self.worker.start()

    def subscribe(self, event_type: str, handler):
        self.subscribers[event_type].append(handler)

    def publish(self, event: Event):
        self.queue.put(event)

    def _run(self):
        while self.running or not self.queue.empty():
            try:
                event = self.queue.get(timeout=0.1)
                for handler in self.subscribers.get(event.type, []):
                    try:
                        handler(event)
                    except Exception as e:
                        print(f"Handler error: {e}")

            except queue.Empty:
                continue

    def shutdown(self):
        self.running = False
        self.worker.join()
