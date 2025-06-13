
from src.api.api import ExchangeAPI
from src.auth.session_manager import SessionManager, VSD
from src.feeds.events import EventBus


EXIT_COMMANDS = ["quit", "q", "exit"]


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
            user_command = input("MiniExchange > ")
            if user_command == "":
                continue

            decoded_command = self._decode_commnand(user_command)

            if decoded_command[0] == "unknowm":
                self.handle_unknown()
                continue

            elif decoded_command[0] == "invalid":
                self.handle_invalid()
                continue

            elif decoded_command[0] == "help":
                self.help()
                continue

            elif decoded_command[0] in EXIT_COMMANDS:
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

                self.output(response)

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
                    args["token"] = parts[1]

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
                    args["token"] = parts[1]
                    args["order_id"] = parts[2]

                case "spread" | "spreadinfo":
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

                case _:
                    request = {"error": "Unable to construct request."}

        except Exception as e:
            request = {"error": f"Ran into unexpected error: {e}"}

        return request

    def handle_unknown(self):
        print("  Unknown command. Type 'help' to see available commands")

    def handle_invalid(self):
        print("""
        Invalid command format or missing arguments. Try again or type 'help'.
              """)

    def help(self):
        print("""
MiniExchange CLI Help:
-------------------------
login <username> <password>                     - Log in to the system
logout <token>                                  - Log out of the system

order <username> <side> <qty> <type> [price]    - Place an order
   • side: buy | sell
   • qty: float
   • type: market | limit
   • price: required if type is limit

cancel <token> <order_id>                       - Cancel an order

spread                                          - View current best bid and ask
spreadinfo                                      - Show detailed spread metrics

help                                            - Show this help message

quit | exit | q                                 - Quit\n""")

    def error(self):
        print("Error: Malformed request or missing required parameters.")

    def output(self, response: dict):
        if response.get("error"):
            print(f"Error: {response["error"]}")
        else:
            print(response)
