# MiniExchange
Single symbol orderbook / exchange prototype in Python.

Design Notes:

Orders:

This is a simplified prototype, but I still wanted semi realistic behavior. An order is represented by a dataclass. I added support for both limit orders and market orders. The difference between the 2 is that with a limit order, the client specifies a price that they wish to buy the asset for, but with the market order, they just specify the quantity, and they will get whatever is the best available price at that moment in the market.

To handle this behavior, I created an abstract Order class and 2 child classes called LimitOrder and MarketOrder, both of which inherit from Order.

To keep track of the market activity, I added a field called "client_id" which help identify market participants later. In the market data stream, there are 2 versions, the private feed contains the client_id's, but the public feed, much like in real systems, the identity of market participants are obfuscated to protect their privacy and to not potentially reveal certain players' trading strategies.

To make life easier and avoid typos, created a side and a status enum:

```python
class OrderStatus(Enum):
    NEW = "new"
    PARTIALLY_FILLED = "partially_filled"
    FILLED = "filled"
    CANCELLED = "cancelled"


class OrderSide(Enum):
    BUY = "buy"
    SELL = "sell"
```

For now, these are the only status messages that are added for simplicity's sake.

Example: LimitOrder class:

```python
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
            order_id=str(uuid4()),
            side=side.value,
            price=price,
            qty=qty
        )
```

To keep the logic of the matching engine as small as possible, an intermediate layer called the order dispatcher was added that does order validation, filtering, and dispatching.

Tomorrow:

- add order cancellation
- add spread calculation
- create a session manager with logins
- simulate fix, fix-json support
- response messages from the api
- market data feed - pub/sub, public, private
- add support for csv reading to handle large amounts of data
- add logs
- rigorous testing
- documentation - write about the core components, how an order flows through the system
- documentation - write about the matching logic, FIFO, data formats
- add performance benchmarking
