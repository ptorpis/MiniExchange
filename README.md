# UNDER CONSTRUCTION... 🚧
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

text for introduction

# Setup and Installation

I developed this project in Python 3.12, I cannot guarantee that it works in earlier versions, but should be fine 3.10+.

## Dependencies
I tried to keep dependencies to a minimum. The libraries needed to run are: `sortedcontainers`
To install, run:
```bash
pip install -r requirements.txt
```

# Usage

# Command Line Interface

# Design
## Orders:

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
## Trades

## Events
Pub-sub system

## API

## Dispatcher

## Authentication and Session Management
---

# MiniExchangeAPI Specs

This API is designed to simulate a trading exchange interface, supporting user login, order submissions (limit/market), cancellations, and market data queries.

## Request Format

Each request is a JSON dictionary with at least:

```json
{
  "type": "request_type",
  "payload": {...}
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
    ...
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

## Testing the API

There is a validation layer built into the API to make sure that bad requests don't crash the system. If a bad request is encountered, the response should contain a "success": False field.

Test cases covered:

1. Tesing logging in.
2. Testing logging out.
3. Sending a bad logout request.
4. Sending a bad limit order. (the order side is not recognized)
5. Sending a nonexistent request type.

## How to Run Tests

The testing framework is Python's built-in `unittest`. There are other great testing frameworks available, but in keeping with the theme of keeping dependencies as lean as possible, I went with the built in option.

The tests are organized into the `tests/` folder, to run everything, just run `test.py`, which is found in the project root.

```bash
python test.py
```
