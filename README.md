# MiniExchange
Single symbol orderbook / exchange prototype in Python.

# Table of Contents
- [Introduction](#introduction)
- [Setup and Installation](#setup-and-installation)
- [Usage](#usage)
- [Command Line Interface](#command-line-interface)
- [Design](#design)
    - [Orders](#orders)
    - [Trades](#trades)
    - [Events](#events)
    - [API](#api)
    - [Dispatcher](#dispatcher)
    - [Authentication and Session Management](#authentication-and-session-management)
- [MiniExchangeAPI Specs](#miniexchangeapi-specs)
    - [Request Format](#request-format)
    - [Session Management](#session-management)
    - [Market Data](#market-data)
    - [Order Management](#order-management)
    - [Error Response Template](#error-response-template)
    - [Sample User Accounts](#sample-user-accounts)
    - [Examples](#examples)
- [Testing Suite](#testing-suite)
    - [Testing Orders](#testing-orders)
    - [How to Run Tests](#how-to-run-tests)
# Introduction

This project is meant to be a prototype for a FIFO order book single symbol exchange. In it's current form, it can serve as a sandbox to run simulations in and test different microstructure behaviors. It's not meant to be production grade, and things like slippage are yet to be added. There are only 2 types of orders, limit and market, more complicated order types are not supported as of now.

Full breakdown in: [PDF](docs/design-and-implementation.pdf)

# Setup and Installation

I developed this project in Python 3.12, I cannot guarantee that it works in earlier versions, but should be fine 3.10+.

## Dependencies
I tried to keep dependencies to a minimum. The only library needed to run is: `sortedcontainers`
To install, run:
```bash
pip install -r requirements.txt
```

# Usage

Run `main.py` and use the CLI to interact with the market order by order. When restarting the application, the order book is reset to an empty state. To place an order, a user has to first log in, the login credentials are currently managed by and in memory database stored in the session manager source file.

To see all commands and their usage, type `help` after running `main.py`.

# Command Line Interface

The command line interface should be treated more like a demo toy than anything, but it works the same way, integrating with the internal API, with some quality of life features. The outputs are formatted, and the user does not have to manually write the client ID tokens, instead, when logged in, a user can just pass their username.

Here is some example output and the help page that shows the usage of the CLI.
After running `main.py`:
```
MiniExchange > help

MiniExchange CLI Help:
--------------------------------------------------------------
 > login <username> <password>                   - Log in

 > logout <username>                             - Log out of the system

 > order <username> <side> <qty> <type> [price]  - Place an order
    - side: buy | sell
    - qty: float
    - type: market | limit
    - price: required if type is limit
             (don't include if it's a market order)

 > cancel <username> <order_id>                  - Cancel an order

 > spread                                        - View current best bid and ask

 > spreadinfo                                    - Show detailed spread metrics

 > book                                          - Print order book with all resting orders

 > help                                          - Show this help message

 > quit | exit | q                               - Quit
--------------------------------------------------------------

MiniExchange > login testuser test
Login Success.
MiniExchange > order testuser sell 100 limit 100

  Order:
      ID: 59d98b31
      Side: sell
      Status: NEW
      Original Quantity: 100.0
      Remaining Quantity: 100.0
      Filled Quantity: 0

  No Trades.

MiniExchange > order testuser buy 100 limit 99

  Order:
      ID: dea94ee9
      Side: buy
      Status: NEW
      Original Quantity: 100.0
      Remaining Quantity: 100.0
      Filled Quantity: 0

  No Trades.

MiniExchange > book
Order Book:
Bids:
  99.00: [id: dea94ee9, qty: 100.0, status: new]
Asks:
  100.00: [id: 59d98b31, qty: 100.0, status: new]
MiniExchange > 

```
After running `main.py`, the user get's promted: `MiniExchange > `, then the user is able to log in, run commands, see the order book, etc.

# Design
## Orders:

In this system there are 2 types of orders that are currently supported. Limit Orders and Market Orders. They share the same base class and inherit from it, the only difference is that the market order does not iclude a price, while a limit order must. When sumbitting a market order, including a price will make the request invalid and will be rejected.

A market order will always execute at the best available price, but if the order book is empty it gets cancelled.

Limit orders are added to price levels and then filled based on FIFO logic.

The order objects are created using static constructors:

```python
LimitOrder.create(client_id, side, price, qty)
MarketOrder.create(client_id, side, qty)
```

Note that this order creation is done by the [dispatcher](#dispatcher) layer.

There are currently 4 possible states an order can be in:

```python
class OrderStatus(Enum):
    NEW = "new"
    PARTIALLY_FILLED = "partially_filled"
    FILLED = "filled"
    CANCELLED = "cancelled"
```

Orders are serializable using their `to_dict()` method.

## Trades

To encapsulate the unit of exchange activities in the system, a Trade dataclass was created. This can store all the information about a single transaction.

```python
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
```

Much like the Orders, the Trade objects are initialized using their `create()` static methods.

For more readable representation, a `__repr__()` function is included.

## Events

The `EventBus` provides an asynchronous, decoupled event-driven communication mechanism within the system, enabling logging and market feeds to exist without impacting the performance of the order book. This system uses publisher-subscribers model to handle decoupled event management.

Events are made up of 2 fields, `type` - the type of event - and `data` which is a dictionary payload about all the relevant information about a certain event.

The event bus creates a new thread and runs in the background. Events are placed in the queue when they are published, then they are handled by whichever component is subscribed to that event. They can also be ignored.

A test mode is also included, this disables thread creation when running tests, this was added because most tests don't need events to be running, but initialization requires an event bus to exist. By passing in `test_mode=True`, the event bus can be disabled.

## API

To see the specs, go [here](#miniexchangeapi-specs)

The `ExchangeAPI` object is the main entry point for a client to place orders, query market data, log in an out. It takes care of request validation, routing and response formatting.

Its responsibilities include:

- Request Handling
- Validation
- Authentication, by integrating with `SessionManager`.
- Data and Request Routing.
- Public Data Access, it provides read-only endpoints, such as order book spread and best bid/ask information without requiring authentication.
- Error Handling: bad requests are handled gracefully.

The response format is kept consistent for ease of use.

## Dispatcher

This is the layer that sits between the API and the order book. It makes sure that the order requests are valid and make sense, then routes them into the order book.

This separation of responsibility is important to make sure that the API only has to handle request format validation, while the dispatcher can take a closer look at the incoming orders before they are sumbitted to the market.

In the dispatcher file, there are custom exceptions defined to help identify the reason the order was not valid.

## Order Book

This is the central piece of the system, it takes care of the matching of orders and execution of trades.

The `OrderBook` maintains two sorted collections of limit orders:

- Bids: Buy orders sorted by descending price (highest first).
- Asks: Sell orders sorted by ascending price (lowest first).

Orders are grouped in price-level queues (FIFO) to preserve time priority at each price. There are other matching policies out there, for this project, FIFO was chosen for it's simplicity.

Key data structures: 

- Bids and asks use `SortedDict`s from `sortedcontainers`, which is the only dependency of this project. This keeps the resting orders sorted per price level.
- Order map: tracks every order, used for cancellations (O(1) lookup for removals)

Matching Logic
### Limit Orders (`_match_limit_order`)

Attempts to match the incoming limit order against the opposite book side.
Matches price levels that satisfy the order’s price condition:
- Buy orders match asks with price ≤ order.price
- Sell orders match bids with price ≥ order.price

Uses FIFO within each price level queue.

Updates order quantities and status (filled, partially_filled).

Emits events:
- TRADE for each match.
- ORDER_FILLED or ORDER_PARTIALLY_FILLED as order status changes.
Unmatched residual quantity is added to the book as a new order.

It is possible to cancel a limit order after it has been placed, all is needed is the client id of the user that placed the order and the order id.

### Market Orders (`_match_market_order`)

- Matches against the opposite book side at the best available prices.
- Continues matching until the order is fully filled or no prices remain.
- If the order cannot be fully matched, it is partially filled or cancelled.
- Emits corresponding events (TRADE, ORDER_FILLED, ORDER_PARTIALLY_FILLED, ORDER_CANCELLED).

## Authentication and Session Management

Docs TBA

---

# MiniExchangeAPI Specs

This API is designed to simulate a trading exchange interface, supporting user login, order submissions (limit/market), cancellations, and market data queries.

## Request Format

Each request is a JSON dictionary with at least:

```json
{
  "type": "request_type",
  "payload": {
        "foo": "bar"
    }
}
```

## Session Management

### `login`

**Request:**

```json
{
  "type": "login",
  "payload": {
    "username": "alice",
    "password": "pwdalice"
  }
}
```

**Response:**

```json
{
  "success": true,
  "token": "abc12345"
}
```

> If credentials are invalid:

```json
{
  "success": false,
  "error": "Invalid credentials"
}
```

> If missing fields:

```json
{
  "success": false,
  "error": "Missing 'username' or 'password' in login request."
}
```

### `logout`

**Request:**

```json
{
  "type": "logout",
  "payload": {
    "token": "abc12345"
  }
}
```

**Response:**

```json
{ "success": true }
```

> If session is invalid or token is missing:

```json
{
  "success": false,
  "error": "Invalid session token."
}
```

```json
{
  "success": false,
  "error": "Missing session token."
}
```

##  Market Data

### `spread`

Returns the best bid/ask spread.

**Request:**

```json
{
  "type": "spread"
}
```

**Response:**

```json
{
  "spread": 0.5
}
```

### `spread_info`

Returns detailed spread info including best bid and ask.

**Request:**

```json
{
  "type": "spread_info"
}
```

**Response:**

```json
{
  "best_bid": 100.0,
  "best_ask": 100.5,
  "spread": 0.5
}
```

## Order Management

### `order`

Authenticated request to place an order.

#### Limit Order

**Request:**

```json
{
  "type": "order",
  "payload": {
    "token": "abc12345",
    "side": "buy",
    "price": 100.0,
    "qty": 10,
    "order_type": "limit"
  }
}
```

#### Market Order

**Request:**

```json
{
  "type": "order",
  "payload": {
    "token": "abc12345",
    "side": "sell",
    "qty": 5,
    "order_type": "market"
  }
}
```

**Response:**

```json
{
  "success": true,
  "result": {
    "order_id": "d3adbeef",
    "side": "sell",
    "original_qty": 5,
    "remaining_qty": 0,
    "filled_qty": 5
    "status": "filled"
  }
}
```

> If there's an issue (e.g. missing fields, invalid token):

```json
{
  "success": false,
  "error": "Unauthorized or missing session token."
}
```

### `cancel`

Cancel an existing order by ID (must be owned by the session's user).

**Request:**

```json
{
  "type": "cancel",
  "payload": {
    "token": "abc12345",
    "order_id": "d3adbeef"
  }
}
```

**Response:**

```json
{
  "success": true
}
```

> If token or order ID is missing or invalid:

```json
{
  "success": false,
  "error": "Missing order_id in cancel request."
}
```

## Error Response Template

Any unexpected behavior or misuse of the API will return:

```json
{
  "success": false,
  "error": "Error message here"
}
```

## Sample User Accounts 

(currently this is being managed by an in memory dictionary)

```json
{
  "alice": "pwdalice",
  "bob": "pwdbob",
  "testuser": "test"
}
```
## Examples

There are example request found in `docs/reference_api_requests.json`. Consult that file to get the format of every type of request.

> [!NOTE]
> - **All authenticated requests** (`order`, `cancel`, `logout`) must include the `token`.
> - Tokens are currently 8-character UUID prefixes for simplicity.
> - Designed for both human CLI interaction and automated CSV-based testing.
> - All state is in-memory (for prototype); no persistence across runs.

# Testing suite

There is a comprehensive testing suite provided that tests the core functionality of the system. Run it to verify that the system is working correctly. Note that this is not guarantee that *everything* works as expected, but can be a pretty good gauge that they are.

The test are broken into separate modules, where each test module is testing the behavior of a module, for example `tests/test_orders.py` is testing placing, filling, cancelling orders and possible edge cases that might arise.

There are most likely scenarios that I have not thought of, but I tried coming up with some critical cases to ensure correctness of behavior.

## Testing Orders

Test cases covered:
1. Placing a market when the order book is empty. This should result in an order cancellation.
2. Placing a limit buy.
3. Placing a limit sell.
4. Placing 2 identical, but opposite side orders and checking if they fill perfectly.
5. Cancelling an order.
6. Placing a limit order with a negative quantity. This should be rejected.
7. Placing a limit order with a negative price. This should be rejected.
8. Placing a limit order with a price of 0. This should be rejected.
9. Testing FIFO at price level. As per the policy of the order book, an order that got to a price level first should be the one getting filled first.
10. Testing that partial fills behave as expected.
11. When placing a market order, it should be filled with the best available price.
12. When a large market order is coming in that can fill multiple resting orders, it should be able to "walk the book".
13. Placing a market order with a price shoudl be rejected.
14. Trying to cancel a nonexistent order should be rejected.
15. Cancelling a filled order should not be possible.
16. Placing 2 identical orders should be recorded as 2, separate orders.
17. Cancelling someone else's order should not be possible.
18. When a limit order crosses the other side's best price, it should first take the better price and execute immediately.

## Testing the API

There is a validation layer built into the API to make sure that bad requests don't crash the system. If a bad request is encountered, the response should contain a "success": False field.

Test cases covered:

1. Tesing logging in.
2. Testing logging out.
3. Sending a bad logout request.
4. Sending a bad limit order. (the order side is not recognized)
5. Sending a nonexistent request type.
6. A request that is not a dict should be handled gracefully.
7. Logging in twice with the same user should get a response with the same token and not reset the session.
8. Logging in with multiple users.
9. Logging out then trying to use the token should not be possible.
10. After logging in and out, the old token should not be valid and the new one should.
11. Passing in a price as a string should be an invalid request.
12. Passing in a quantity as a string should be an invalid request.

## Testing the CLI

The testing of the CLI was done by hand to make sure that the format of the output is correct and that invalid requests are not getting through.

## How to Run Tests

The testing framework is Python's built-in `unittest`. There are other great testing frameworks available, but in keeping with the theme of keeping dependencies as lean as possible, I went with the built in option.

The tests are organized into the `tests/` folder, to run everything, just run `test.py`, which is found in the project root.

```bash
python test.py
```
