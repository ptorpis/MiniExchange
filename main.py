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

cancel_request = exchange.cancel_order(result.get('result').order_id)
print(cancel_request)

print(exchange.dispatcher.order_book)

order_request = {
    "client_id": "foo",
    "side": "buy",
    "qty": 10,
    "price": 1000,
    "type": "limit"
}
result = exchange.submit_order(order_request)

order_request = {
    "client_id": "foo",
    "side": "sell",
    "qty": 10,
    "price": 1000.01,
    "type": "limit"
}
result = exchange.submit_order(order_request)

print(exchange.dispatcher.order_book.get_spread())
