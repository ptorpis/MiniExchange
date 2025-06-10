from src.dispatcher import OrderDispatcher

disp = OrderDispatcher()

order_msg = {
    "type": "limit",
    "client_id": "foo",
    "side": "buy",
    "price": 100.0,
    "qty": 1.0
}

order = disp.dispatch(order_msg)
print(order)
print(disp.order_book)
cancellation = disp.cancel_order("foo")
print(disp.order_book)
print(cancellation)
cancellation = disp.cancel_order(order.order_id)
print(disp.order_book)
print(cancellation)
