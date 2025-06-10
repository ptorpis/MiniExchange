from src.dispatcher import OrderDispatcher, InvalidOrderError


class ExchangeAPI:
    def __init__(self):
        self.dispatcher = OrderDispatcher()

    def submit_order(self, order_msg: dict) -> dict:
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
                "error": str(e)
            }

    def cancel_order(self, order_id: str) -> dict:
        result = self.dispatcher.cancel_order(order_id)

        return {
            "success": result
        }

    def get_spread(self) -> dict:
        result = self.dispatcher.order_book.get_spread()

        return {
            "spread": result
        }
