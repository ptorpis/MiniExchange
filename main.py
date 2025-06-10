from src.api import ExchangeAPI

exchange = ExchangeAPI()

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
print(result)
print(exchange.dispatcher.order_book)
