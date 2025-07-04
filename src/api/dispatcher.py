from src.core.order import Order, LimitOrder, MarketOrder
from src.core.order_book import OrderBook
from src.core.trade import Trade


class InvalidOrderError(Exception):
    """
    Base class for invalid order exceptions, these include

    - invalid side
    - missing field
    - invalid quantity
    - invalid price
    - order type mismatch (including a price for market orders)
    - unsupported order type
    """
    pass


class InvalidOrderSideError(InvalidOrderError):
    def __init__(self, side):
        super().__init__(f"Invalid order side: '{side}'")


class MissingFieldError(InvalidOrderError):
    def __init__(self, field):
        super().__init__(f"Missing required field: '{field}'")


class InvalidQuantityError(InvalidOrderError):
    def __init__(self, qty):
        super().__init__(f"Invalid quantity: '{qty}'")


class InvalidPriceError(InvalidOrderError):
    def __init__(self, price):
        super().__init__(f"Invalid price: '{price}'")


class OrderTypeMismatchError(InvalidOrderError):
    def __init__(self, order_type, has_price):
        msg = (
            f"'{order_type}' should not include a price"
            if order_type == 'market' and has_price
            else f"'{order_type}' should include a price"
        )

        super().__init__(msg)


class UnsupportedOrderTypeError(InvalidOrderError):
    def __init__(self, order_type):
        super().__init__(f"Unsupported order type: '{order_type}'")


def validate_order(payload: dict):
    required_fields = ["client_id", "side", "qty", "order_type"]

    for field in required_fields:
        if field not in payload:
            raise MissingFieldError(field)

    side = payload["side"]
    if side not in ("buy", "sell"):
        raise InvalidOrderSideError(side)

    qty = payload["qty"]
    if not isinstance(qty, (int, float)) or qty <= 0:
        raise InvalidQuantityError(qty)

    price = payload.get("price")
    order_type = payload["order_type"]

    if order_type == "limit":
        if price is None:
            raise OrderTypeMismatchError("limit", has_price=False)
        if not isinstance(price, (int, float)) or price <= 0:
            raise InvalidPriceError(price)

    elif order_type == "market":
        if price is not None:
            raise OrderTypeMismatchError("market", has_price=True)
    else:
        raise UnsupportedOrderTypeError(order_type)


class OrderDispatcher:
    def __init__(self, event_bus=None):
        self.even_bus = event_bus
        self.order_book = OrderBook(event_bus=event_bus)

    def dispatch(self, order_msg: dict) -> Order:
        validate_order(order_msg)

        if order_msg["order_type"] == "limit":
            order = LimitOrder.create(
                client_id=order_msg["token"],
                side=order_msg["side"],
                price=round(order_msg["price"], 2),
                qty=order_msg["qty"]
            )
        elif order_msg["order_type"] == "market":
            order = MarketOrder.create(
                client_id=order_msg["token"],
                side=order_msg["side"],
                qty=order_msg["qty"],
            )
        else:
            raise InvalidOrderError(f"Unknown order type: {order_msg["type"]}")
        trades = self.order_book.match(order)
        formatted_order = return_format_order(order, order_msg)
        formatted_trades = return_format_trades(trades)
        return {
            "order": formatted_order,
            "trades": formatted_trades
        }

    def cancel_order(self, order_id: str, client_id):
        return self.order_book.cancel_order(order_id, client_id)


def return_format_order(order: Order, order_msg: dict):
    order_id = order.order_id
    side = order.side

    original_qty = order_msg["qty"]

    if order.status == "filled":
        remaining_qty = 0
        filled_qty = order.qty
    elif order.status == "partially_filled":
        remaining_qty = order.qty
        filled_qty = original_qty - remaining_qty
    elif order.status == "cancelled":
        remaining_qty = order.qty
        filled_qty = 0
    elif order.status == "new":
        remaining_qty = order.qty
        filled_qty = 0
    else:
        raise ValueError("Unknown order status.")

    return {
        "order_id": order_id,
        "status": order.status,
        "original_qty": original_qty,
        "remaining_qty": remaining_qty,
        "filled_qty": filled_qty,
        "side": side,
    }


def return_format_trades(trades: list[Trade]):
    if not trades:
        return []

    formatted = []

    for trade in trades:
        info = {
            "trade_id": trade.trade_id,
            "price": trade.price,
            "qty": trade.qty,
            "timestamp": trade.timestamp,
        }
        formatted.append(info)

    return formatted
