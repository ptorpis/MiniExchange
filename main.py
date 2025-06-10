import time

from src.api import ExchangeAPI
from src.events import EventBus
from src.logger import EventLogger
# from src.feeds import PublicFeed
from src.feeds import PrivateFeed

# logger = EventLogger('logs/events.jsonl')

event_bus = EventBus()
"""
event_bus.subscribe("ORDER_ADDED", logger)
event_bus.subscribe("ORDER_CANCELLED", logger)
event_bus.subscribe("TRADE", logger)
event_bus.subscribe("ORDER_PARTIALLY_FILLED", logger)
"""
exchange = ExchangeAPI(event_bus=event_bus)
# feed = PublicFeed(event_bus=event_bus)
feed = PrivateFeed(event_bus=event_bus)

request = {
    "type": "order",
    "payload": {
        "client_id": "foo",
        "side": "buy",
        "qty": 10,
        "price": 100.0,
        "order_type": "limit"
    }
}

result = exchange.handle_request(request)
time.sleep(1)
cancel = {
    "type": "cancel",
    "payload": {
        "order_id": result.get("result").order_id
    }
}

result = exchange.handle_request(cancel)
time.sleep(1)
request = {
    "type": "order",
    "payload": {
        "client_id": "foo",
        "side": "buy",
        "qty": 10,
        "price": 100.0,
        "order_type": "limit"
    }
}

result = exchange.handle_request(request)

time.sleep(1)
request = {
    "type": "order",
    "payload": {
        "client_id": "foo",
        "side": "sell",
        "qty": 5,
        "price": 100.0,
        "order_type": "limit"
    }
}

result = exchange.handle_request(request)

time.sleep(1)
# print(exchange.dispatcher.order_book)
event_bus.shutdown()
# logger.close()
