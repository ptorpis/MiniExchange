from src.api.dispatcher import OrderDispatcher, InvalidOrderError


class ExchangeAPI:
    def __init__(self, event_bus):
        self.event_bus = event_bus
        self.dispatcher = OrderDispatcher(event_bus=event_bus)

    def handle_request(self, request: dict) -> dict:
        request_type = request.get("type")
        payload = request.get("payload", {})

        if request_type == "order":
            return self._submit_order(payload)

        elif request_type == "cancel":
            order_id = payload.get("order_id")
            if not order_id:
                return {
                    "success": False,
                    "error": "Missing order_id in cancel request."
                }
            return self._cancel_order(order_id)

        elif request_type == "spread":
            return self._get_spread()

        elif request_type == "spread_info":
            return self._get_spread_info()

        else:
            return {
                "success": False,
                "error": f"Unknown request type: '{request_type}'"
            }

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

    def _cancel_order(self, order_id: str) -> dict:
        result = self.dispatcher.cancel_order(order_id)

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
            "spead": self.dispatcher.order_book.get_spread()
        }
