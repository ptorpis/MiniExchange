import json
from datetime import datetime

from src.events import Event, EventBus


class PrivateFeed:
    def __init__(self, event_bus: EventBus):
        self.event_bus = event_bus

        self.event_bus.subscribe("ORDER_ADDED", self._handle_event)
        self.event_bus.subscribe("ORDER_CANCELLED", self._handle_event)
        self.event_bus.subscribe("TRADE", self._handle_event)
        self.event_bus.subscribe("PRDER_PARTIALLY_FILLED", self._handle_event)
        self.event_bus.subscribe("ORDER_FILLED", self._handle_event)

    def _handle_event(self, event: Event):
        output = {
            "timestamp": datetime.utcnow().isoformat(),
            "event_type": event.type,
            "data": event.data
        }

        print(json.dumps(output, indent=2))


class PublicFeed:
    def __init__(self, event_bus: EventBus):
        self.event_bus = event_bus

        self.event_bus.subscribe("ORDER_ADDED", self._handle_event)
        self.event_bus.subscribe("ORDER_CANCELLED", self._handle_event)
        self.event_bus.subscribe("TRADE", self._handle_event)
        self.event_bus.subscribe("PRDER_PARTIALLY_FILLED", self._handle_event)
        self.event_bus.subscribe("ORDER_FILLED", self._handle_event)

    def _handle_event(self, event: Event):
        if event.type == "TRADE":
            event.data["seller_id"] = "***"
            event.data["buyer_id"] = "***"
            event.data["seller_order_id"] = "***"
            event.data["buyer_order_id"] = "***"
        else:
            event.data["client_id"] = "***"
            event.data["order_id"] = "***"

        output = {
            "timestamp": datetime.utcnow().isoformat(),
            "event_type": event.type,
            "data": event.data
        }

        print(json.dumps(output, indent=2))
