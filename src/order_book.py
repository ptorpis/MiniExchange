from collections import deque
from sortedcontainers import SortedDict

from src.order import LimitOrder


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

    def add_order(self, order: LimitOrder):
        book_side = self.bids if order.side == "buy" else self.asks

        if order.price not in book_side:
            book_side[order.price] = deque()

        book_side[order.price].append(order)

    def __repr__(self):
        return (
            "Order Book:\n"
            f"  Bids: {list(self.bids.items())}\n"
            f"  Asks: {list(self.asks.items())}\n"
        )
