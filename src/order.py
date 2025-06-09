from dataclasses import dataclass
from enum import Enum
from uuid import uuid4
import time


class OrderStatus(Enum):
    NEW = "new"
    PARTIALLY_FILLED = "partially_filled"
    FILLED = "filled"
    CANCELLED = "cancelled"


class OrderSide(Enum):
    BUY = "buy"
    SELL = "sell"


@dataclass
class Order:
    client_id: str
    order_id: str
    side: OrderSide
    price: float
    qty: float
    status: OrderStatus = OrderStatus.NEW
    timestamp: float = time.time()

    @staticmethod
    def create(client_id: str, side: OrderSide, price: float, qty: float) -> "Order":
        if isinstance(side, str):
            side = OrderSide(side.lower())
        return Order(
            client_id=client_id,
            order_id=str(uuid4()),
            side=side,
            price=price,
            qty=qty
        )

    def to_dict(self) -> dict:
        return {
            "order_id": self.order_id,
            "side": self.side.value,
            "price": self.price,
            "qty": self.qty,
            "status": self.status.value,
            "timestamp": self.timestamp
        }

    def __repr__(self):
        return (f"Order("
                f"client_id={self.client_id}, "
                f"id={self.order_id}, "
                f"side={self.side.value}, "
                f"price={self.price}, "
                f"quantity={self.qty}, "
                f"status={self.status.value}, "
                f"timestamp={self.timestamp})")
