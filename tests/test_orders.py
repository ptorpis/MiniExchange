import unittest

from src.api.api import ExchangeAPI
from src.auth.session_manager import SessionManager, VSD
from src.feeds.events import EventBus


class TestOrderHandling(unittest.TestCase):

    def setUp(self):
        self.eb = EventBus()
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

    def test_login(self):
        # test if login is successful
        login_request = {
            "type": "login",
            "payload": {
                "username": "testuser",
                "password": "test"
            }
        }

        result = self.exch.handle_request(login_request)
        token = result.get("token")
        self.assertIsNotNone(token)
        self.eb.shutdown()

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
        self.eb.shutdown()

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
        self.eb.shutdown()

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
        self.eb.shutdown()

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
        self.eb.shutdown()

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
        self.eb.shutdown()
