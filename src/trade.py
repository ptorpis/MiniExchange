import time
from uuid import uuid4
from dataclasses import dataclass


@dataclass
class Trade:
    trade_id: str
    price: float
    qty: float
    timestamp: float
    buyer_order_id: str
    seller_order_id: str
    buyer_id: str
    seller_id: str

    @staticmethod
    def create(
        price: float,
        qty: float,
        buyer_order_id: str,
        seller_order_id: str,
        buyer_id: str,
        seller_id: str
    ) -> "Trade":
        return Trade(
            trade_id=str(uuid4()),
            price=price,
            qty=qty,
            timestamp=time.time(),
            buyer_order_id=buyer_order_id,
            seller_order_id=seller_order_id,
            buyer_id=buyer_id,
            seller_id=seller_id
        )

    def to_dict(self) -> dict:
        return {
            "trade_id": self.trade_id,
            "price": self.price,
            "qty": self.qty,
            "timestamp": self.timestamp,
            "buyer_order_id": self.buyer_order_id,
            "seller_order_id": self.seller_order_id,
            "buyer_id": self.buyer_id,
            "seller_id": self.seller_id
        }

    def __repr__(self):
        return ("Trade("
                f"trade_id={self.trade_id}, "
                f"price={self.price}, "
                f"quantity={self.qty}, "
                f"timestamp={self.timestamp}, "
                f"buyer_order_id={self.buyer_order_id}, "
                f"seller_order_id={self.seller_order_id}, "
                f"buyer_id={self.buyer_id}, "
                f"seller_id={self.seller_id})")
