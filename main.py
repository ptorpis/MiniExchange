from src.api import ExchangeAPI

exchange = ExchangeAPI()

order_request = {
    "client_id": "foo",
    "side": "buy",
    "qty": 10,
    "price": 1000,
    "type": "limit"
}

result = exchange.submit_order(order_request)
print(result)

order_request = {
    "client_id": "foo",
    "side": "sell",
    "qty": 10,
    "price": 1000,
    "type": "limit"
}

result = exchange.submit_order(order_request)

print(result)
print(exchange.dispatcher.order_book)
