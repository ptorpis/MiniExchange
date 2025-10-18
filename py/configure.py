import json
from datetime import date, datetime
import argparse

BUILD_CHOICES = ["debug", "release", "relwithdebug"]


def load_defaults():
    with open("config/default_config.json", "r") as dcfg:
        return json.load(dcfg)


def promt_with_default(name: str, default: str):
    promt_text = f"Enter {name}. (press ENTER to accept default={default})"
    user_input = input(promt_text).strip()

    if user_input == "":
        user_input = default

    return user_input


def main():
    parser = argparse.ArgumentParser(description="Configure MiniExchange.")
    parser.add_argument(
        "--interactive", action="store_true", help="Configure interactively."
    )

    parser.add_argument("--build-type", choices=BUILD_CHOICES)
    parser.add_argument("--port", type=int)
    parser.add_argument("--name", type=str, help="Optional run name")
    args = parser.parse_args()

    if args.interactive:
        build_type = promt_with_default("build-type", "debug")
    else:
        build_type = "rel"

    if args.name:
        name = args.name
    else:
        name = "default"

    config: dict = {"build-type": build_type, "name": name}
    print(config)


if __name__ == "__main__":
    main()
