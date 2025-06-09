from src.order_book import OrderBook
from src.order import LimitOrder, Order, MarketOrder
from src.order import OrderSide

if __name__ == "__main__":
    order_book = OrderBook()

    limit_sell = LimitOrder.create(
        client_id="seller1",
        side=OrderSide.SELL,
        price=100.0,
        qty=5
    )
    order_book.match(limit_sell)  # Should not produce a trade yet

    # Add a limit buy order that matches
    limit_buy = LimitOrder.create(
        client_id="buyer1",
        side=OrderSide.BUY,
        price=91.0,
        qty=5
    )
    trades = order_book.match(limit_buy)

    print("Trades:", trades)
    print("Order Book:", order_book)

    limit_sell = LimitOrder.create(
        client_id="seller1",
        side=OrderSide.SELL,
        price=100.0,
        qty=5
    )
    order_book.match(limit_sell)  # Should not produce a trade yet

    # Add a limit buy order that matches
    limit_buy = LimitOrder.create(
        client_id="buyer1",
        side=OrderSide.BUY,
        price=100.0,
        qty=5
    )
    trades = order_book.match(limit_buy)

    print("Trades:", trades)
    print("Order Book:", order_book)
