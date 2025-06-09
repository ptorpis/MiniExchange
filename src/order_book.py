from collections import deque
from sortedcontainers import SortedDict

from src.order import LimitOrder, MarketOrder, Order
from src.order import OrderSide, OrderStatus
from src.trade import Trade


class OrderBook:
    def __init__(self):
        """
        Using sortedcontainers to keep the prices sorted.
        """
        self.bids = SortedDict()
        self.asks = SortedDict()

    def best_bid(self):
        if not self.bids:
            return None
        return self.bids.peekitem(-1)[0]  # Highest bid will be the last

    def best_ask(self):
        if not self.asks:
            return None
        return self.asks.peekitem(0)[0]  # Lowest ask will be the first

    def add_order(self, order: Order):
        if not isinstance(order, LimitOrder):
            raise ValueError("Only LimitOrders can be added to the book.")

        book_side = self.bids if order.side == "buy" else self.asks

        if order.price not in book_side:
            book_side[order.price] = deque()

        book_side[order.price].append(order)

    def match(self, order: Order):
        if isinstance(order, LimitOrder):
            return self._match_limit_order(order)

        elif isinstance(order, MarketOrder):
            return self._match_market_order(order)

    def _match_limit_order(self, order: LimitOrder):
        trades = []
        remaining_qty = order.qty
        if order.side == OrderSide.BUY.value:
            while remaining_qty > 0 and len(self.asks) > 0:
                best_price = self.best_ask()
                if best_price > order.price:
                    break

                ask_queue = self.asks[best_price]

                while ask_queue and remaining_qty > 0:
                    best_ask = ask_queue[0]
                    match_qty = min(remaining_qty, best_ask.qty)

                    trade = Trade.create(
                        price=best_price,
                        qty=match_qty,
                        buyer_order_id=order.order_id,
                        seller_order_id=best_ask.order_id,
                        buyer_id=order.client_id,
                        seller_id=best_ask.client_id
                    )

                    trades.append(trade)

                    best_ask.qty -= match_qty
                    remaining_qty -= match_qty

                    if best_ask.qty == 0:
                        ask_queue.popleft()
                    else:
                        best_ask.status = OrderStatus.PARTIALLY_FILLED.value

                if not ask_queue:
                    del self.asks[best_price]

        elif order.side == OrderSide.SELL.value:
            while remaining_qty > 0 and len(self.bids) > 0:
                best_price = self.best_bid()

                if best_price < order.price:
                    break

                bid_queue = self.bids[best_price]
                while bid_queue and remaining_qty > 0:
                    best_bid = bid_queue[0]
                    match_qty = min(remaining_qty, best_bid.qty)

                    trade = Trade.create(
                        price=best_price,
                        qty=match_qty,
                        buyer_order_id=best_bid.order_id,
                        seller_order_id=order.order_id,
                        buyer_id=best_bid.client_id,
                        seller_id=order.client_id
                    )

                    trades.append(trade)

                    best_bid.qty -= match_qty
                    remaining_qty -= match_qty

                    if best_bid.qty == 0:
                        best_bid.status = OrderStatus.FILLED.value
                        bid_queue.popleft()
                    else:
                        best_bid.status = OrderStatus.PARTIALLY_FILLED.value

                if not bid_queue:
                    del self.bids[best_price]

        if remaining_qty == 0:
            order.status = OrderStatus.FILLED.value
        elif remaining_qty < order.qty:
            order.status = OrderStatus.PARTIALLY_FILLED.value
            order.qty = remaining_qty
            self.add_order(order)
        else:
            self.add_order(order)

        return trades

    def _match_market_order(self, order: MarketOrder):
        remaining_qty = order.qty
        trades = []

        if order.side == OrderSide.BUY.value:
            book = self.asks
        else:
            book = self.bids

        while remaining_qty > 0 and len(book) > 0:
            best_price = next(iter(book))
            queue = book[best_price]

            while queue and remaining_qty > 0:
                resting_order = queue[0]
                match_qty = min(remaining_qty, resting_order.qty)

                if order.side == OrderSide.BUY.value:
                    buyer_id = order.client_id
                    buyer_order_id = order.order_id
                    seller_id = resting_order.client_id
                    seller_order_id = resting_order.order_id
                else:
                    buyer_id = resting_order.client_id
                    buyer_order_id = resting_order.order_id
                    seller_id = order.client_id
                    seller_order_id = order.order_id

                trade = Trade.create(
                    price=best_price,
                    qty=match_qty,
                    buyer_order_id=buyer_order_id,
                    seller_order_id=seller_order_id,
                    buyer_id=buyer_id,
                    seller_id=seller_id
                )
                trades.append(trade)

                resting_order.qty -= match_qty
                remaining_qty -= match_qty

                if resting_order.qty == 0:
                    queue.popleft()

            if not queue:
                del book[best_price]

        if remaining_qty == 0:
            order.status = OrderStatus.FILLED.value
        elif remaining_qty < order.qty:
            order.status = OrderStatus.PARTIALLY_FILLED.value
            order.qty = remaining_qty
        else:
            order.status = OrderStatus.CANCELLED.value

        return trades

    def __repr__(self):
        def format_side(side):
            lines = []
            for price, queue in side.items():
                orders_str = ', '.join(
                    f"[id={order.order_id[:6]} qty={order.qty} "
                    f"status={order.status}]"
                    for order in queue
                )
                lines.append(f"  {price:.2f}: {orders_str}")
            return '\n'.join(lines) if lines else "  []"

        return (
            "Order Book:\n"
            "Bids:\n" + format_side(self.bids) + "\n" +
            "Asks:\n" + format_side(self.asks)
        )

