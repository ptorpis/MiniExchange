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

        self.order_map: dict[str, tuple[str, float, deque]] = {}

    def best_bid(self):
        if not self.bids:
            return None
        return self.bids.peekitem(-1)[0]  # Highest bid will be the last

    def best_ask(self):
        if not self.asks:
            return None
        return self.asks.peekitem(0)[0]  # Lowest ask will be the first

    def _add_order(self, order: Order):
        if not isinstance(order, LimitOrder):
            raise ValueError("Only LimitOrders can be added to the book.")

        book_side = self.bids if order.side == "buy" else self.asks

        if order.price not in book_side:
            book_side[order.price] = deque()

        # book_side[order.price].append(order)

        queue = book_side[order.price]
        queue.append(order)

        self.order_map[order.order_id] = (order.side, order.price, queue)

    def match(self, order: Order):
        if isinstance(order, LimitOrder):
            return self._match_limit_order(order)

        elif isinstance(order, MarketOrder):
            return self._match_market_order(order)

    def _match_limit_order(self, order: LimitOrder):
        """
        Using FIFO logic, best prices are matched, then filled and partially
        filler orders are updated.
        """
        trades = []
        remaining_qty = order.qty

        if order.side == OrderSide.BUY.value:
            book = self.asks
            asks = True
        else:
            book = self.bids
            asks = False

        while remaining_qty > 0 and len(book) > 0:
            if asks:
                best_price = self.best_ask()
            else:
                best_price = self.best_bid()

            if asks:
                if best_price > order.price:
                    break
            elif best_price < order.price:
                break

            queue = book[best_price]

            while queue and remaining_qty > 0:
                resting_order = queue[0]
                match_qty = min(remaining_qty, resting_order.qty)

                trade = Trade.create(
                    price=best_price,
                    qty=match_qty,
                    buyer_order_id=order.order_id,
                    seller_order_id=resting_order.order_id,
                    buyer_id=order.client_id,
                    seller_id=resting_order.client_id
                )

                trades.append(trade)

                resting_order.qty -= match_qty
                remaining_qty -= match_qty

                if resting_order.qty == 0:
                    queue.popleft()
                else:
                    resting_order.status = OrderStatus.PARTIALLY_FILLED.value

            if not queue:
                del book[best_price]

        if remaining_qty == 0:
            order.status = OrderStatus.FILLED.value
        elif remaining_qty < order.qty:
            order.status = OrderStatus.PARTIALLY_FILLED.value
            order.qty = remaining_qty
            self._add_order(order)
        else:
            self._add_order(order)

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

    def cancel_order(self, order_id: str) -> bool:
        entry = self.order_map.get(order_id)

        if not entry:
            return False

        side, price, queue = entry

        for i, order in enumerate(queue):
            if order.order_id == order_id:
                order.status = OrderStatus.CANCELLED.value
                del queue[i]

                if not queue:
                    if side == OrderSide.BUY.value:
                        book = self.bids
                    else:
                        book = self.asks

                    del book[price]
                del self.order_map[order_id]
                return True

        return False

    def get_spread(self) -> float | None:
        best_bid = self.best_bid()
        best_ask = self.best_ask()

        if best_bid is None or best_ask is None:
            return None

        return round(best_ask - best_bid, 6)

    def __repr__(self):
        def format_side(side):
            lines = []
            for price, queue in side.items():
                orders_str = ', '.join(
                    f"[id: {order.order_id[:6]}, qty: {order.qty}, "
                    f"status: {order.status}]"
                    for order in queue
                )
                lines.append(f"  {price:.2f}: {orders_str}")
            return '\n'.join(lines) if lines else "  []"

        return (
            "Order Book:\n"
            "Bids:\n" + format_side(self.bids) + "\n" +
            "Asks:\n" + format_side(self.asks)
        )
