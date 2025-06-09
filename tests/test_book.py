from src.order_book import OrderBook
from src.order import LimitOrder, MarketOrder, OrderSide

if __name__ == "__main__":
    order_book = OrderBook()

    limit_sell = LimitOrder.create(
        client_id="seller1",
        side=OrderSide.SELL,
        price=100.0,
        qty=5
    )
    order_book.match(limit_sell)  # Should not produce a trade yet

    limit_buy = LimitOrder.create(
        client_id="buyer1",
        side=OrderSide.BUY,
        price=100.0,
        qty=3
    )
    trades = order_book.match(limit_buy)

    print("Trades:", trades)
    print("Order Book:", order_book)

    limit_buy = LimitOrder.create(
        client_id="buyer1",
        side=OrderSide.BUY,
        price=99.0,
        qty=3
    )
    trades = order_book.match(limit_buy)

    print("Trades:", trades)
    print("Order Book:", order_book)

    limit_buy = LimitOrder.create(
        client_id="buyer1",
        side=OrderSide.BUY,
        price=98.0,
        qty=3
    )
    trades = order_book.match(limit_buy)

    print("Trades:", trades)
    print("Order Book:", order_book)

    limit_sell = LimitOrder.create(
        client_id="seller1",
        side=OrderSide.SELL,
        price=95.0,
        qty=6
    )
    trades = order_book.match(limit_sell)
    print("Trades:", trades)
    print("Order Book:", order_book)
