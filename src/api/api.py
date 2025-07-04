from src.api.dispatcher import OrderDispatcher, InvalidOrderError
from src.auth.session_manager import SessionManager
from src.feeds.events import EventBus
from src.api.validators import validators, ValidationError


class ExchangeAPI:
    def __init__(self, event_bus: EventBus, session_manager: SessionManager):
        self.event_bus = event_bus
        self.session_manager = session_manager
        self.dispatcher = OrderDispatcher(event_bus=event_bus)

    def handle_request(self, request: dict) -> dict:
        if type(request) is not dict:
            return {
                "success": False,
                "error": "Malformed request."
            }

        request_type = request.get("type")
        payload = request.get("payload", {})

        if request_type is None or payload is None:
            return {
                "success": False,
                "error": "'request_type' or 'payload' not found."
            }

        try:
            validator = validators.get(request_type)
            if validator:
                validator(payload)
        except ValidationError as e:
            return {
                "success": False,
                "error": f"Invalid request: {e}"
            }

        match request_type:
            case "spread" | "spread_info":
                return self._handle_public_request(request_type)

            case "login":
                return self._login(payload)

            case "order" | "cancel":
                user = self._get_user(request.get("payload"))
                if not user:
                    return {
                        "success": False,
                        "error": "Unauthorized."
                    }

                if request_type == "order":
                    payload["client_id"] = user
                    return self._submit_order(payload)

                if request_type == "cancel":
                    order_id = payload.get("order_id")
                    client_id = payload.get("token")
                    if not order_id or not client_id:
                        return {
                            "success": False,
                            "error": "Missing 'client_id' or 'order_id' "
                                     "in cancel request."
                        }
                    return self._cancel_order(order_id, client_id)

            case "logout":
                return self._logout(payload)

            case "book":
                return {
                    "success": True,
                    "result": str(self.dispatcher.order_book)
                }
            case _:
                return {
                    "success": False,
                    "error": f"Unknown request type: {request_type}"
                }

    def _handle_public_request(self, request_type: str):
        if request_type == "spread":
            return self._get_spread()

        elif request_type == "spread_info":
            return self._get_spread_info()

    def _submit_order(self, order_msg: dict) -> dict:
        try:
            result = self.dispatcher.dispatch(order_msg)

            return {
                "success": True,
                "result": result
            }
        except InvalidOrderError as e:
            return {
                "success": False,
                "error": str(e)
            }

        except Exception as e:
            return {
                "success": False,
                "error": f"Internal Server Error {e}"
            }

    def _cancel_order(self, order_id: str, client_id) -> dict:
        result = self.dispatcher.cancel_order(order_id, client_id)

        return {
            "success": result
        }

    def _get_spread(self) -> dict:
        result = self.dispatcher.order_book.get_spread()

        return {
            "spread": result
        }

    def _get_spread_info(self) -> dict:
        return {
            "best_bid": self.dispatcher.order_book.best_bid(),
            "best_ask": self.dispatcher.order_book.best_ask(),
            "spread": self.dispatcher.order_book.get_spread()
        }

    def _get_user(self, payload: dict) -> str | None:
        token = payload.get("token")
        if not token:
            return None
        return self.session_manager.get_user(token)

    def _login(self, payload: dict) -> dict:
        username = payload.get("username")
        password = payload.get("password")

        if not username or not password:
            return {
                "success": False,
                "error": "Missing 'username' or 'password' in login request."
            }

        token = self.session_manager.login(username, password)

        if token:
            return {
                "success": True,
                "token": token
            }
        else:
            return {
                "success": False,
                "error": "Invalid credentials"
            }

    def _logout(self, payload: dict) -> dict:
        token = payload.get("token")

        if not token:
            return {
                "success": False,
                "error": "Missing session token."
            }

        success = self.session_manager.logout(token)

        if success:
            return {
                "success": success,
            }
        else:
            return {
                "success": False,
                "error": "Invalid session token."
            }
        return self.session_manager.logout(token)
