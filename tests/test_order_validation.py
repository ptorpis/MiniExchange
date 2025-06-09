import unittest
from src.dispatcher import validate_order
from src.dispatcher import (
    InvalidOrderError,
    MissingFieldError,
    InvalidOrderSideError,
    InvalidQuantityError,
    InvalidPriceError,
)


class TestOrderValidation(unittest.TestCase):
    def test_valid_limit_order(self):
        order = {
            "client_id": "foo",
            "side": "buy",
            "price": 99.9,
            "qty": 10,
            "type": "limit"
        }
        try:
            validate_order(order)
        except InvalidOrderError:
            self.fail("validate_order() raised an exception unexpectedly "
                      "for a valid limit order.")

    def test_valid_market_order(self):
        order = {
            "client_id": "bar",
            "side": "sell",
            "qty": 5,
            "type": "market"
        }
        try:
            validate_order(order)
        except InvalidOrderError:
            self.fail("validate_order() raised an exception unexpectedly "
                      "for a valid market order.")

    def test_missing_type(self):
        order = {
            "client_id": "foo",
            "side": "buy",
            "price": 100,
            "qty": 10
        }
        with self.assertRaises(MissingFieldError):
            validate_order(order)

    def test_invalid_side(self):
        order = {
            "client_id": "foo",
            "side": "hold",
            "price": 100,
            "qty": 10,
            "type": "limit"
        }
        with self.assertRaises(InvalidOrderSideError):
            validate_order(order)

    def test_negative_qty(self):
        order = {
            "client_id": "foo",
            "side": "buy",
            "price": 100,
            "qty": -5,
            "type": "limit"
        }
        with self.assertRaises(InvalidQuantityError):
            validate_order(order)

    def test_zero_price_limit_order(self):
        order = {
            "client_id": "foo",
            "side": "sell",
            "price": 0,
            "qty": 10,
            "type": "limit"
        }
        with self.assertRaises(InvalidPriceError):
            validate_order(order)


if __name__ == '__main__':
    unittest.main()
