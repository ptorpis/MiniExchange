import unittest

from src.api.api import ExchangeAPI
from src.auth.session_manager import SessionManager, VSD
from src.feeds.events import EventBus


class TestOrderHandling(unittest.TestCase):

    def setUp(self):
        self.eb = EventBus(test_mode=True)
        self.sess = SessionManager(user_db=VSD)
        self.exch = ExchangeAPI(event_bus=self.eb, session_manager=self.sess)

    def tearDown(self):
        self.exch.dispatcher.order_book.asks.clear()
        self.exch.dispatcher.order_book.bids.clear()
        self.exch.dispatcher.order_book.order_map.clear()

    def login(self):
        login_request = {
            "type": "login",
            "payload": {
                "username": "testuser",
                "password": "test"
            }
        }

        result = self.exch.handle_request(login_request)
        token = result.get("token")
        return token

    def test_market_order_empty_ob(self):
        # A market order into an empty order book should be cancelled.
        token = self.login()

        market_order_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "qty": 1.0,
                "order_type": "market"
            }
        }

        response = self.exch.handle_request(market_order_request)
        result = response.get("result")
        self.assertEqual(result.status, "cancelled")

    def test_place_limit_buy_order(self):
        token = self.login()

        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 100.0,
                "qty": 1.0,
                "order_type": "limit"
            }
        }

        response = self.exch.handle_request(buy_limit_request)
        self.assertTrue(response.get("success"))

    def test_place_limit_sell_order(self):
        token = self.login()

        sell_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "sell",
                "price": 100.0,
                "qty": 1.0,
                "order_type": "limit"
            }
        }

        response = self.exch.handle_request(sell_limit_request)
        self.assertTrue(response.get("success"))

    def test_fill_order(self):
        # Placing 2 indentical, but opposite limit
        # orders should fill each other.
        # After they are filled, the order book should be empty.
        token = self.login()

        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 100.0,
                "qty": 1.0,
                "order_type": "limit"
            }
        }

        self.exch.handle_request(buy_limit_request)

        sell_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "sell",
                "price": 100.0,
                "qty": 1.0,
                "order_type": "limit"
            }
        }

        self.exch.handle_request(sell_limit_request)
        self.assertFalse(self.exch.dispatcher.order_book.bids)
        self.assertFalse(self.exch.dispatcher.order_book.asks)

    def test_cancel_order(self):
        # After placing a single order and cancelling it, the order book should
        # be empty.

        token = self.login()

        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 100.0,
                "qty": 1.0,
                "order_type": "limit"
            }
        }

        response = self.exch.handle_request(buy_limit_request)

        order_id = response.get("result").order_id

        cancel_request = {
            "type": "cancel",
            "payload": {
                "token": token,
                "order_id": order_id
            }
        }

        self.exch.handle_request(cancel_request)
        self.assertFalse(self.exch.dispatcher.order_book.bids)

    def test_negative_limit_qty(self):
        token = self.login()

        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 1.0,
                "qty": -1.0,
                "order_type": "limit"
            }
        }

        response = self.exch.handle_request(buy_limit_request)
        self.assertFalse(response.get("success"))

    def test_negative_limit_price(self):
        token = self.login()

        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": -1.0,
                "qty": 1.0,
                "order_type": "limit"
            }
        }

        response = self.exch.handle_request(buy_limit_request)
        self.assertFalse(response.get("success"))

    def test_zero_price(self):
        token = self.login()

        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 0.0,
                "qty": 1.0,
                "order_type": "limit"
            }
        }

        response = self.exch.handle_request(buy_limit_request)
        self.assertFalse(response.get("success"))

    def test_fifo_at_price_level(self):
        token = self.login()

        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 100.0,
                "qty": 10.0,
                "order_type": "limit"
            }
        }

        self.exch.handle_request(buy_limit_request)

        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 100.0,
                "qty": 1.0,
                "order_type": "limit"
            }
        }

        second_response = self.exch.handle_request(buy_limit_request)
        second_order_id = second_response["result"].order_id

        sell_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "sell",
                "price": 100.0,
                "qty": 10.0,
                "order_type": "limit"
            }
        }

        self.exch.handle_request(sell_limit_request)
        remaining_orders = self.exch.dispatcher.order_book.bids.get(100.0, [])
        self.assertEqual(len(remaining_orders), 1)
        remaining_order = remaining_orders[0]
        self.assertEqual(remaining_order.order_id, second_order_id)

    def test_partial_fill(self):
        token = self.login()
        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 100.0,
                "qty": 10.0,
                "order_type": "limit"
            }
        }
        self.exch.handle_request(buy_limit_request)
        sell_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "sell",
                "price": 100.0,
                "qty": 8.0,
                "order_type": "limit"
            }
        }
        self.exch.handle_request(sell_limit_request)
        correct_qty = 2
        remaining_order = self.exch.dispatcher.order_book.bids\
            .get(100.0, [])[0]
        self.assertEqual(correct_qty, remaining_order.qty)

    def test_market_order_price_priority(self):
        token = self.login()
        buy_limit_request = {  # higher price buy order
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 101.0,
                "qty": 10.0,
                "order_type": "limit"
            }
        }
        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 100.0,
                "qty": 10.0,
                "order_type": "limit"
            }
        }
        self.exch.handle_request(buy_limit_request)
        market_order_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "sell",
                "qty": 1.0,
                "order_type": "market"
            }
        }
        self.exch.handle_request(market_order_request)
        remaining_order = self.exch.dispatcher.order_book.bids\
            .get(100.0, [])[0]
        correct_qty = 9
        self.assertEqual(correct_qty, remaining_order.qty)

    def test_filling_multiple_orders_with_market_order(self):
        token = self.login()
        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 100.0,
                "qty": 10.0,
                "order_type": "limit"
            }
        }
        self.exch.handle_request(buy_limit_request)
        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 100.0,
                "qty": 10.0,
                "order_type": "limit"
            }
        }
        self.exch.handle_request(buy_limit_request)
        market_order_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "sell",
                "qty": 19.0,
                "order_type": "market"
            }
        }
        response = self.exch.handle_request(market_order_request)
        self.assertEqual(response.get("success"), True)
