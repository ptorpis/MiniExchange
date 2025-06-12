from typing import Callable


class ValidationError(Exception):
    pass


def validate_login(payload):
    if "username" not in payload or "password" not in payload:
        raise ValidationError("Missing 'username' or 'password'.")


def validate_order(payload):
    required = ["token", "side", "qty", "order_type"]

    for field in required:
        if field not in payload:
            raise ValidationError(f"Missing required field. '{field}'.")

    if payload["side"] not in ["buy", "sell"]:
        raise ValidationError("Invalid value for 'side'.")

    if payload["order_type"] == "limit":
        if "price" not in payload:
            raise ValidationError("Limit orders must have a 'price'.")


def validate_cancel(payload):
    if "token" not in payload or "order_id" not in payload:
        raise ValidationError("Missing 'token' or 'order_type'")


def validate_logout(payload):
    if "token" not in payload:
        raise ValidationError("Missing 'token' from logout request.")


validators: dict[str, Callable] = {
    "login": validate_login,
    "order": validate_order,
    "cancel": validate_cancel,
    "logout": validate_logout,
    "spread": lambda p: None,
    "spread_info": lambda p: None
}
