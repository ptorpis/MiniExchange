from datetime import datetime

from src.feeds.events import Event, EventBus


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
            "event_type": event.type,
            "timestamp": datetime.utcnow().isoformat(),
            "data": event.data
        }

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
            "event_type": event.type,
            "timestamp": datetime.utcnow().isoformat(),
            "data": event.data
        }

        print("-"*80)
        print("", end="| ")
        for item in output.items():
            if not isinstance(item[1], dict):
                print(item[1], end=" | ")
            else:
                print()
                for dataitem in item[1].items():
                    if dataitem[0] in [
                        "client_id",
                        "order_id",
                        "seller_id",
                        "buyer_id",
                        "seller_order_id",
                        "buyer_order_id"
                    ]:
                        continue
                    print(f"    {dataitem[0]}: {dataitem[1]}")
