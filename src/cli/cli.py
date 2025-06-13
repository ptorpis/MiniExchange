from enum import Enum

from src.api.api import ExchangeAPI
from src.auth.session_manager import SessionManager, VSD
from src.feeds.events import EventBus


EXIT_COMMANDS = ["quit", "q", "exit"]


class COLORS(Enum):
    CYAN = "\033[96m"
    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    MAGENTA = "\033[95m"
    RESET = "\033[0m"


class CLI:
    def __init__(self):
        self.event_bus = EventBus()
        self.session_manager = SessionManager(user_db=VSD)
        self.api = ExchangeAPI(
            event_bus=self.event_bus,
            session_manager=self.session_manager
        )

    def mainloop(self):
        while True:
            user_command = input(
                f"{COLORS.CYAN.value}MiniExchange{COLORS.RESET.value} > "
            )

            if user_command == "":
                continue

            decoded_command = self._decode_commnand(user_command)
            command_name = decoded_command[0]

            if command_name == "unknowm":
                self.handle_unknown()
                continue

            elif command_name == "invalid":
                self.handle_invalid()
                continue

            elif command_name == "help":
                self.help()
                continue

            elif command_name in EXIT_COMMANDS:
                print("Exiting.")
                self.event_bus.shutdown()
                break

            else:
                request = self._construct_request(decoded_command)
                if request.get("error") is None:
                    response = self.api.handle_request(request)
                else:
                    self.error()
                    continue

                self.output(response, command_name)

    def _decode_commnand(self, input_str: str) -> tuple[str, dict]:
        parts = input_str.strip().split()
        if not parts:
            return "", {}

        command = parts[0]
        args = {}

        try:
            match command:
                case "login":
                    args["username"] = parts[1]
                    args["password"] = parts[2]

                case "logout":
                    username = parts[1]
                    token = self.session_manager.get_token(username)
                    args["token"] = token

                case "order":
                    username = parts[1]
                    token = self.session_manager.get_token(username)
                    args["token"] = token
                    args["side"] = parts[2]
                    args["qty"] = float(parts[3])
                    args["order_type"] = parts[4]

                    if args["order_type"] == "limit":
                        args["price"] = float(parts[5])

                case "cancel":
                    username = parts[1]
                    token = self.session_manager.get_token(username)
                    args["token"] = token
                    args["order_id"] = parts[2]

                case "spread" | "spreadinfo":
                    pass

                case "book":
                    pass

                case "quit" | "q" | "exit":
                    pass

                case "help":
                    pass

                case _:
                    command = "unknown"

        except (IndexError, ValueError):
            command = "invalid"

        return command, args

    def _construct_request(self, command: tuple[str, dict]) -> dict:
        request = {}

        command_type = command[0]
        args = command[1]

        try:
            match command_type:
                case "login":
                    request = {
                        "type": "login",
                        "payload": {
                            "username": args["username"],
                            "password": args["password"]
                        }
                    }

                case "logout":
                    request = {
                        "type": "logout",
                        "payload": {
                            "token": args["token"]
                        }
                    }

                case "order":
                    if args["order_type"] == "limit":
                        request = {
                            "type": "order",
                            "payload": {
                                "token": args["token"],
                                "side": args["side"],
                                "price": args["price"],
                                "qty": args["qty"],
                                "order_type": args["order_type"]
                            }
                        }
                    elif args["order_type"] == "market":
                        request = {
                            "type": "order",
                            "payload": {
                                "token": args["token"],
                                "side": args["side"],
                                "qty": args["qty"],
                                "order_type": args["order_type"]
                            }
                        }
                case "cancel":
                    request = {
                        "type": "cancel",
                        "payload": {
                            "token": args["token"],
                            "order_id": args["order_id"]
                        }
                    }

                case "spread":
                    request = {
                        "type": "spread"
                    }

                case "spreadinfo":
                    request = {
                        "type": "spread_info"
                    }

                case "book":
                    request = {
                        "type": "book"
                    }

                case _:
                    request = {"error": "Unable to construct request."}

        except Exception as e:
            request = {"error": f"Ran into unexpected error: {e}"}

        return request

    def handle_unknown(self):
        print("Unknown command. Type 'help' to see available commands")

    def handle_invalid(self):
        print("Invalid command format or missing arguments. "
              "Try again or type 'help'.")

    def help(self):
        print(
            f"\n{COLORS.CYAN.value}MiniExchange CLI Help:{COLORS.RESET.value}"
            f"\n--------------------------------------------------------------"
            f"\n > "
            f"login <username> <password>                   - Log in\n"
            f"\n > "
            f"logout <username>                             - Log out "
            f"of the system\n"
            f"\n > "
            f"order <username> <side> <qty> <type> [price]  - Place an order\n"
            f"    - side: buy | sell\n"
            f"    - qty: float\n"
            f"    - type: market | limit\n"
            f"    - price: required if type is limit\n"
            f"             (don't include if it's a market order)\n"
            f"\n > "
            f"cancel <username> <order_id>                  - Cancel "
            f"an order\n"
            f"\n > "
            f"spread                                        - View current "
            f"best bid and ask\n"
            f"\n > "
            f"spreadinfo                                    - Show detailed "
            f"spread metrics\n"
            f"\n > "
            f"book                                          - Print order "
            f"book with all resting orders\n"
            f"\n > "
            f"help                                          - Show this "
            f"help message\n"
            f"\n > "
            f"quit | exit | q                               - Quit\n"
            f"--------------------------------------------------------------"
            f"\n"
        )

    def error(self):
        print("Error: Malformed request or missing required parameters.")

    def output(self, response: dict, command_name):
        if response.get("error"):
            print(f"Error: {response["error"]}")
        else:
            formatted_output = self._format_response(response, command_name)
            print(formatted_output)

    def _format_response(self, response, command_type) -> str:
        fmt_rsp: str
        match command_type:
            case "login":
                fmt_rsp = "Login Success."

            case "logout":
                fmt_rsp = "Logout Success."

            case "order":
                result = response.get("result")
                order = result["order"]
                trades = result["trades"]
                formatted_order = self.__format_order(order)
                formatted_trades = self.__format_trades(trades)
                fmt_rsp = formatted_order + "\n" + formatted_trades

            case "cancel":
                fmt_rsp = "Order Cancelled."

            case "spread":
                current_spread = response.get("spread")
                if current_spread is None:
                    current_spread = "order book is empty."

                fmt_rsp = (
                    f"\n  Current Bid-Ask Spread: "
                    f"{current_spread}\n"
                )

            case "spreadinfo":
                current_spread = response.get("spread")
                if current_spread is None:
                    current_spread = "Order book is empty."

                best_bid = response.get("best_bid")
                if best_bid is None:
                    best_bid = "Bids are empty."

                best_ask = response.get("best_ask")
                if best_ask is None:
                    best_ask = "Asks are empty."

                fmt_rsp = (
                    f"\n  Current Market Spread Information: \n"
                    f"      Current Bid-Ask Spread: {current_spread}\n"
                    f"      Current Best Bid: {best_bid}\n"
                    f"      Current Best Ask: {best_ask}\n"
                )

            case "book":
                fmt_rsp = response.get("result")

            case _:
                fmt_rsp = "Formatting not implemented."

        return fmt_rsp

    def __format_order(self, order):
        return (
            f"\n  Order:\n"
            f"      ID: {order['order_id']}\n"
            f"      Side: {order['side']}\n"
            f"      Status: {order['status'].upper()}\n"
            f"      Original Quantity: {round(order['original_qty'], 3)}\n"
            f"      Remaining Quantity: {round(order['remaining_qty'], 3)}\n"
            f"      Filled Quantity: {round(order['filled_qty'], 3)}\n"
        )

    def __format_trades(self, trades):
        if len(trades) == 0:
            return "  No Trades.\n"

        formatted_list = []

        for i in range(len(trades)):
            formatted_list.append(
                f"  Trade[{i + 1}]:\n"
                f"      Trade ID: {trades[i]['trade_id']}\n"
                f"      Price: {trades[i]['price']}\n"
                f"      Quantity: {trades[i]['qty']}\n"
                f"      Timestamp: {trades[i]['timestamp']}\n"
            )

        fmt_trds = "\n".join(formatted_list)
        return fmt_trds
