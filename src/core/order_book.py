from collections import deque
from sortedcontainers import SortedDict
from datetime import datetime, timezone

from src.core.order import LimitOrder, MarketOrder, Order
from src.core.order import OrderSide, OrderStatus
from src.core .trade import Trade
from src.feeds.events import EventBus, Event


class OrderBook:
    def __init__(self, event_bus: EventBus = None):
        """
        Using sortedcontainers to keep the prices sorted.

        Methods:
        Public:     - best_bid() -> float | None
                        returns the best available bid price, or None if the
                        book is empty

                    - best_ask() -> float | None
                        returns the best available ask price, or None if the
                        book is empty

                    - cancel_order(order_id) -> bool
                        cancels the order, removes it from the order map and
                        the bids/asks, returns bool to indicate success or fail

                    - get_spread() -> float | None
                        returns the current market spread or None if the
                        book (at least one of the bid/ask) is empty

                    - match(order: Order)
                        routes the order to the correct matching function

        'Private':  - _match_limit_order(order: LimitOrder) -> list[Trade]
                        matching logic for limit orders
                        retunrs a list of trades triggered by the limit
                        order, updates order status accordingly

                    - _match_market_order(order: MarketOrder) -> list[Trade]
                        matching logic for the market orders, if no matching
                        price is found, the order is cancelled
                        similarly to the _match_limit_order, it returns a
                        list of the trades triggered by the incoming order
                        and updates the order statuses accordingly

                    - _add_order(order: Order)
                        adds an order to the bids/asks
                        this method is called internally, when the order cannot
                        be matched
        """
        self.bids = SortedDict()
        self.asks = SortedDict()

        self.order_map: dict[str, tuple[str, float, deque]] = {}

        self.event_bus = event_bus

    def best_bid(self):
        if not self.bids:
            return None
        return self.bids.peekitem(-1)[0]  # Highest bid will be the last

    def best_ask(self):
        if not self.asks:
            return None
        return self.asks.peekitem(0)[0]  # Lowest ask will be the first

    def cancel_order(self, order_id: str) -> bool:
        entry = self.order_map.get(order_id)

        if not entry:
            return False

        side, price, queue = entry

        for i, order in enumerate(queue):
            if order.order_id == order_id:
                order.status = OrderStatus.CANCELLED.value
                del queue[i]

                event = Event(
                    type="ORDER_CANCELLED",
                    data={
                        "order_id": order_id,
                        "client_id": order.client_id,
                        "side": side,
                        "price": price,
                        "qty": order.qty,
                        "timestamp": datetime.utcnow().isoformat()
                    }
                )
                if self.event_bus:
                    self.event_bus.publish(event)

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

                self.event_bus.publish(Event(
                    type="TRADE",
                    data={
                        "price": best_price,
                        "qty": match_qty,
                        "buyer_order_id": buyer_order_id,
                        "seller_order_id": seller_order_id,
                        "buyer_id": buyer_id,
                        "seller_id": seller_id,
                        "timestamp": datetime.now(timezone.utc).isoformat()
                    }
                ))

                resting_order.qty -= match_qty
                remaining_qty -= match_qty

                if resting_order.qty == 0:
                    resting_order.status = OrderStatus.FILLED.value

                    self.event_bus.publish(Event(
                        type="ORDER_FILLED",
                        data={
                            "order_id": resting_order.order_id,
                            "side": resting_order.side,
                            "price": best_price,
                            "qty": match_qty,
                            "client_id": resting_order.client_id,
                            "timestamp": datetime.now(timezone.utc).isoformat()
                        }
                    ))
                    queue.popleft()
                else:
                    resting_order.status = OrderStatus.PARTIALLY_FILLED.value
                    self.event_bus.publish(Event(
                        type="ORDER_PARTIALLY_FILLED",
                        data={
                            "order_id": resting_order.order_id,
                            "side": resting_order.side,
                            "price": best_price,
                            "qty": match_qty,
                            "client_id": resting_order.client_id,
                            "timestamp": datetime.now(timezone.utc).isoformat()
                        }
                    ))

            if not queue:
                del book[best_price]

        if remaining_qty == 0:
            order.status = OrderStatus.FILLED.value
            self.event_bus.publish(Event(
                type="ORDER_FILLED",
                data={
                    "order_id": order.order_id,
                    "side": order.side,
                    "price": order.price,
                    "qty": order.qty,
                    "client_id": order.client_id,
                    "timestamp": datetime.now(timezone.utc).isoformat()
                }
            ))

        elif remaining_qty < order.qty:
            order.status = OrderStatus.PARTIALLY_FILLED.value
            order.qty = remaining_qty

            self.event_bus.publish(Event(
                type="ORDER_PARTIALLY_FILLED",
                data={
                    "order_id": order.order_id,
                    "side": order.side,
                    "price": order.price,
                    "qty": order.qty - remaining_qty,
                    "client_id": order.client_id,
                    "timestamp": datetime.now(timezone.utc).isoformat()
                }
            ))

            self._add_order(order)  # logging happens inside this method

        else:
            self._add_order(order)  # Same here

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

                self.event_bus.publish(Event(
                    type="TRADE",
                    data={
                        "price": best_price,
                        "qty": match_qty,
                        "buyer_order_id": buyer_order_id,
                        "seller_order_id": seller_order_id,
                        "buyer_id": buyer_id,
                        "seller_id": seller_id,
                        "timestamp": datetime.now(timezone.utc).isoformat()
                    }
                ))

                resting_order.qty -= match_qty
                remaining_qty -= match_qty

                resting_order.status = OrderStatus.PARTIALLY_FILLED.value

                if resting_order.qty == 0:
                    queue.popleft()

            if not queue:
                del book[best_price]

        if remaining_qty == 0:
            order.status = OrderStatus.FILLED.value
            self.event_bus.publish(Event(
                type="ORDER_FILLED",
                data={
                    "order_id": order.order_id,
                    "side": order.side,
                    "qty": order.qty,
                    "client_id": order.client_id,
                    "timestamp": datetime.now(timezone.utc).isoformat()
                }
            ))
        elif remaining_qty < order.qty:
            order.status = OrderStatus.PARTIALLY_FILLED.value
            order.qty = remaining_qty

            self.event_bus.publish(Event(
                type="ORDER_PARTIALLY_FILLED",
                data={
                    "order_id": order.order_id,
                    "side": order.side,
                    "qty": order.qty - remaining_qty,
                    "client_id": order.client_id,
                    "timestamp": datetime.now(timezone.utc).isoformat()
                }
            ))
        else:
            order.status = OrderStatus.CANCELLED.value

            self.event_bus.publish(Event(
                type="ORDER_CANCELLED",
                data={
                    "order_id": order.order_id,
                    "side": order.side,
                    "qty": order.qty,
                    "client_id": order.client_id,
                    "timestamp": datetime.now(timezone.utc).isoformat()
                }
            ))

        return trades

    def _add_order(self, order: Order):
        if not isinstance(order, LimitOrder):
            raise ValueError("Only LimitOrders can be added to the book.")

        book_side = self.bids if order.side == "buy" else self.asks

        if order.price not in book_side:
            book_side[order.price] = deque()

        queue = book_side[order.price]
        queue.append(order)

        self.order_map[order.order_id] = (order.side, order.price, queue)

        event = Event(
            type="ORDER_ADDED",
            data={
                "order_id": order.order_id,
                "side": order.side,
                "price": order.price,
                "qty": order.qty,
                "client_id": order.client_id,
                "timestamp": datetime.now(timezone.utc).isoformat()
            }
        )

        self.event_bus.publish(event)

    def __repr__(self):
        def format_side(side):
            lines = []
            for price, queue in side.items():
                orders_str = ', '.join(
                    f"[id: {order.order_id[:8]}, qty: {order.qty}, "
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
