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
