import unittest

from src.api.api import ExchangeAPI
from src.auth.session_manager import SessionManager, VSD
from src.feeds.events import EventBus


class TestAPI(unittest.TestCase):

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
        self.eb.shutdown()

    def test_bad_logout(self):
        token = self.login()

        bad_logout_request = {
            "typ": "logout",
            "payload": token
        }

        response = self.exch.handle_request(bad_logout_request)
        self.assertFalse(response.get("success"))
        self.eb.shutdown()

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
        self.eb.shutdown()

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
        self.eb.shutdown()
