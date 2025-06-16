from datetime import datetime
import threading
import logging

from src.feeds.events import Event, EventBus

logger = logging.getLogger("EventFeed")
logger.setLevel(logging.INFO)

handler = logging.FileHandler("logs/event_feed.log", mode='w')
formatter = logging.Formatter('%(asctime)s | %(levelname)s | %(message)s')
handler.setFormatter(formatter)
logger.addHandler(handler)


class PrivateFeed:
    print_lock = threading.Lock()

    def __init__(self, event_bus: EventBus, print=False):
        self.event_bus = event_bus

        self.event_bus.subscribe("ORDER_ADDED", self._handle_event)
        self.event_bus.subscribe("ORDER_CANCELLED", self._handle_event)
        self.event_bus.subscribe("TRADE", self._handle_event)
        self.event_bus.subscribe("PRDER_PARTIALLY_FILLED", self._handle_event)
        self.event_bus.subscribe("ORDER_FILLED", self._handle_event)
        self.print_enable = print

    def _handle_event(self, event: Event):
        output = {
            "event_type": event.type,
            "timestamp": datetime.utcnow().isoformat(),
            "data": event.data
        }

        logger.info(f"[PRIVATE] {output}")

        if self.print_enable:
            with self.print_lock:
                print("-"*80)
                print("", end="| ")
                for item in output.items():
                    if not isinstance(item[1], dict):
                        print(item[1], end=" | ")
                    else:
                        print()
                        for dataitem in item[1].items():
                            print(f"    {dataitem[0]}: {dataitem[1]}")


class PublicFeed:
    print_lock = threading.Lock()

    def __init__(self, event_bus: EventBus, print=False):
        self.event_bus = event_bus

        for event_type in [
            "ORDER_ADDED", "ORDER_CANCELLED", "TRADE",
            "ORDER_PARTIALLY_FILLED", "ORDER_FILLED"
        ]:
            self.event_bus.subscribe(event_type, self._handle_event)

        self.print_enable = print

    def _handle_event(self, event: Event):
        redacted_data = event.data.copy()

        if event.type == "TRADE":
            for k in ["seller_id",
                      "buyer_id",
                      "seller_order_id",
                      "buyer_order_id"]:

                redacted_data[k] = "***"
        else:
            for k in ["client_id", "order_id"]:
                redacted_data[k] = "***"

        output = {
            "event_type": event.type,
            "timestamp": datetime.utcnow().isoformat(),
            "data": redacted_data
        }

        logger.info(f"[PUBLIC] {output}")

        if self.print_enable:
            with self.print_lock:
                print("-" * 80)
                print("", end="| ")
                for key, val in output.items():
                    if not isinstance(val, dict):
                        print(val, end=" | ")
                    else:
                        print()
                        for k, v in val.items():
                            if k in {
                                "client_id", "order_id",
                                "seller_id", "buyer_id",
                                "seller_order_id", "buyer_order_id"
                            }:
                                continue
                            print(f"    {k}: {v}")
