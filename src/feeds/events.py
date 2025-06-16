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
    def __init__(self, test_mode: bool = False, num_workers: int = 2):
        self.subscribers: DefaultDict[
                str, list[Callable[[Event], None]]
            ] = defaultdict(list)
        self.queue = queue.Queue()
        self.running = not test_mode
        self.workers: list[threading.Thread] = []
        self.test_mode = test_mode

        if not test_mode:
            for _ in range(num_workers):
                worker = threading.Thread(target=self._run, daemon=True)
                self.workers.append(worker)
                worker.start()

    def subscribe(self, event_type: str, handler):
        self.subscribers[event_type].append(handler)

    def publish(self, event: Event):
        if not self.test_mode:
            self.queue.put(event)

    def _run(self):
        while self.running or not self.queue.empty():
            try:
                event = self.queue.get(timeout=0.01)
                for handler in self.subscribers.get(event.type, []):
                    try:
                        handler(event)
                    except Exception as e:
                        print(f"Handler error: {e}")
                self.queue.task_done()

            except queue.Empty:
                continue

    def shutdown(self):
        self.running = False
        for worker in self.workers:
            worker.join()
