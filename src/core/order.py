from dataclasses import dataclass, field
from enum import Enum
from uuid import uuid4
import time
from abc import ABC


class OrderStatus(Enum):
    NEW = "new"
    PARTIALLY_FILLED = "partially_filled"
    FILLED = "filled"
    CANCELLED = "cancelled"


class OrderSide(Enum):
    BUY = "buy"
    SELL = "sell"


@dataclass
class Order(ABC):
    client_id: str
    order_id: str
    side: OrderSide
    qty: float
    status: OrderStatus = field(init=False, default=OrderStatus.NEW.value)
    timestamp: float = field(init=False)

    def __post_init__(self):
        self.status = OrderStatus.NEW.value
        self.timestamp = time.time()

    def to_dict(self) -> dict:
        d = {
            "client_id": self.client_id,
            "order_id": self.order_id,
            "side": self.side.value,
            "qty": self.qty,
            "status": self.status,
            "timestamp": self.timestamp,
        }

        if hasattr(self, 'price'):
            d['price'] = self.price
        else:
            d['price'] = None

        return d


@dataclass
class LimitOrder(Order):
    price: float

    @staticmethod
    def create(
        client_id: str,
        side: OrderSide,
        price: float,
        qty: float
    ) -> "LimitOrder":

        if isinstance(side, str):
            side = OrderSide(side.lower())
        return LimitOrder(
            client_id=client_id,
            order_id=str(uuid4())[:8],
            side=side.value,
            price=price,
            qty=qty
        )


@dataclass
class MarketOrder(Order):

    @staticmethod
    def create(
        client_id: str,
        side: OrderSide,
        qty: float
    ) -> "MarketOrder":

        if isinstance(side, str):
            side = OrderSide(side.lower())
        return MarketOrder(
            client_id=client_id,
            order_id=str(uuid4())[:8],
            side=side.value,
            qty=qty
        )
