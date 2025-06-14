import unittest

from src.api.api import ExchangeAPI
from src.auth.session_manager import SessionManager, VSD
from src.feeds.events import EventBus


class TestAPI(unittest.TestCase):

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

    def test_logout(self):
        token = self.login()

        logout_request = {
            "type": "logout",
            "payload": {
                "token": token
            }
        }

        response = self.exch.handle_request(logout_request)

        self.assertTrue(response.get("success"))

    def test_bad_logout(self):
        token = self.login()

        bad_logout_request = {
            "typ": "logout",
            "payload": token
        }

        response = self.exch.handle_request(bad_logout_request)
        self.assertFalse(response.get("success"))

    def test_bad_limit_order(self):
        token = self.login()

        bad_limit_order = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "hold",
                "price": 100.0,
                "qty": 1.0,
                "order_type": "limit"
            }
        }
        respnse = self.exch.handle_request(bad_limit_order)
        self.assertFalse(respnse.get("success"))

    def test_non_existent_request_type(self):
        token = self.login()

        bad_limit_order = {
            "type": "sell",
            "payload": {
                "token": token,
                "side": "sell",
                "price": 100.0,
                "qty": 1.0,
                "order_type": "limit"
            }
        }
        respnse = self.exch.handle_request(bad_limit_order)
        self.assertFalse(respnse.get("success"))

    def test_request_not_dict(self):
        bad_request = "order"

        result = self.exch.handle_request(bad_request)
        self.assertFalse(result["success"])

    def test_multiple_logins(self):
        token1 = self.login()
        token2 = self.login()

        self.assertEqual(token1, token2)

    def test_multiple_users_login(self):
        login_request1 = {
            "type": "login",
            "payload": {
                "username": "testuser",
                "password": "test"
            }
        }
        login_request2 = {
            "type": "login",
            "payload": {
                "username": "alice",
                "password": "pwdalice"
            }
        }
        self.exch.handle_request(login_request1)
        self.exch.handle_request(login_request2)

        self.assertEqual(len(self.exch.session_manager.user_tokens), 2)

    def test_invalid_token_cancel(self):
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
        order_id = response["result"]["order"]["order_id"]

        logout_request = {
            "type": "logout",
            "payload": {
                "token": token
            }
        }
        self.exch.handle_request(logout_request)
        self.assertEqual(len(self.exch.session_manager.user_tokens), 0)

        cancel_order = {
            "type": "cancel",
            "payload": {
                "token": token,
                "order_id": order_id
            }
        }
        response2 = self.exch.handle_request(cancel_order)
        self.assertFalse(response2["success"])
        self.assertEqual(len(self.exch.dispatcher.order_book.bids), 1)

    def test_new_token_rebinding(self):
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
        logout_request = {
            "type": "logout",
            "payload": {
                "token": token
            }
        }
        self.exch.handle_request(logout_request)
        self.assertEqual(len(self.exch.session_manager.user_tokens), 0)
        new_token = self.login()
        order_with_old_token = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 100.0,
                "qty": 1.0,
                "order_type": "limit"
            }
        }
        self.exch.handle_request(order_with_old_token)
        resting_orders = self.exch.dispatcher.order_book.bids.get(100, [])
        self.assertEqual(len(resting_orders), 1)
        order_with_new_token = {
            "type": "order",
            "payload": {
                "token": new_token,
                "side": "buy",
                "price": 100.0,
                "qty": 1.0,
                "order_type": "limit"
            }
        }
        self.exch.handle_request(order_with_new_token)
        resting_orders = self.exch.dispatcher.order_book.bids.get(100, [])
        self.assertEqual(len(resting_orders), 2)

    def test_price_string(self):
        token = self.login()

        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": "100.0",
                "qty": 1.0,
                "order_type": "limit"
            }
        }
        respoonse = self.exch.handle_request(buy_limit_request)
        self.assertFalse(respoonse["success"])
        self.assertEqual(len(self.exch.dispatcher.order_book.bids), 0)

    def test_qty_string(self):
        token = self.login()

        buy_limit_request = {
            "type": "order",
            "payload": {
                "token": token,
                "side": "buy",
                "price": 100.0,
                "qty": "1.0",
                "order_type": "limit"
            }
        }
        respoonse = self.exch.handle_request(buy_limit_request)
        self.assertFalse(respoonse["success"])
        self.assertEqual(len(self.exch.dispatcher.order_book.bids), 0)
