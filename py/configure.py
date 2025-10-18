import json
import argparse
import socket
import random
import time
import string
from pathlib import Path

BUILD_CHOICES = ["Debug", "Release", "RelWithDebugInfo"]
MIN_PORT = 1024
MAX_PORT = 65535
MAX_NAME_LEN = 64

DEFAULTS = {"port": 12345, "build-type": "debug", "output": "output/runs"}


class FlagProvidedAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        setattr(namespace, self.dest, values)
        setattr(namespace, f"{self.dest}_provided", True)


def is_port_free(port: int) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        return s.connect_ex(("127.0.0.1", port)) != 0


def generate_default_run_name():
    timestamp = time.strftime("%Y%m%d-%H%M%S")  # 20251018-142530 example
    suffix = "".join(random.choices(string.ascii_lowercase + string.digits, k=4))
    return f"run-{timestamp}-{suffix}"


def load_build_config():
    build_cfg_path = Path("config/build_config.json")
    if not build_cfg_path.exists():
        raise FileNotFoundError("Build config not found. Run build_helper.py first.")
    with open(build_cfg_path, "r") as f:
        return json.load(f)


def prompt_with_default(name: str, default):
    prompt_text = f"Enter {name} (Press ENTER to accept default={default}): "
    user_input = input(prompt_text).strip()
    return user_input if user_input else default


def check_args(args: dict, defaults: dict):
    if args["port"] < MIN_PORT or args["port"] > MAX_PORT:
        print(
            f"Warning: Port {args['port']} is out of range ({MIN_PORT}-{MAX_PORT})."
            f" Using default port {defaults.get('port', 12345)}."
        )
        args["port"] = defaults.get("port", 12345)

    if not is_port_free(args["port"]):
        raise ValueError(
            f"Error: Port {args['port']} is already in use. Please choose a different port."
        )

    if len(args["name"]) > MAX_NAME_LEN:
        print(
            f"Warning: Run name '{args['name']}' exceeds {MAX_NAME_LEN} characters. "
            f"Using default name."
        )
        args["name"] = generate_default_run_name()

    if args["build-type"] not in BUILD_CHOICES:
        print(
            f"Warning: Invalid build type '{args['build-type']}'. "
            f"Using default '{defaults.get('build-type', 'debug')}'."
        )
        args["build-type"] = defaults.get("build-type", "debug")

    return args


def main():
    build_cfg = load_build_config()

    parser = argparse.ArgumentParser(description="Configure MiniExchange.")
    parser.add_argument(
        "--interactive",
        "-i",
        action="store_true",
        help="Run in interactive configuration mode.",
    )
    parser.add_argument(
        "--build-type",
        "-b",
        choices=BUILD_CHOICES,
        action=FlagProvidedAction,
        default=DEFAULTS.get("build-type", "debug"),
        help="Specify the build type (debug/release/relwithdebug).",
    )
    parser.add_argument(
        "--port",
        "-p",
        type=int,
        action=FlagProvidedAction,
        default=DEFAULTS.get("port", 12345),
        help=f"Specify the port to run the exchange on (range {MIN_PORT}-{MAX_PORT}).",
    )
    parser.add_argument(
        "--name",
        "-n",
        type=str,
        action=FlagProvidedAction,
        default=generate_default_run_name(),
        help=f"Optional run name (max {MAX_NAME_LEN} chars).",
    )

    args = parser.parse_args()

    if args.interactive:
        print("\n--- Interactive Configuration Mode ---\n")

        build_type = (
            args.build_type
            if getattr(args, "build_type_provided", False)
            else prompt_with_default("build type", build_cfg.get("build_type"))
        )
        port = (
            args.port
            if getattr(args, "port_provided", False)
            else int(prompt_with_default("port", DEFAULTS.get("port", 12345)))
        )
        name = (
            args.name
            if getattr(args, "name_provided", False)
            else prompt_with_default("run name", generate_default_run_name())
        )

        print("\nConfiguration collected from user input.")

    else:
        print("\n--- Auto Configuration Mode ---\n")
        build_type = args.build_type
        port = args.port
        name = args.name
        print("Using provided or default values.")

    output_base = Path(DEFAULTS["output"])

    configuration = {
        "build-type": build_type,
        "port": port,
        "name": name,
        "output_base": str(output_base.resolve()),
        "build_metadata": build_cfg,
    }

    configuration = check_args(configuration, DEFAULTS)

    print("\n--- Final Configuration ---")
    print(f"Build type      : {configuration['build-type']}")
    print(f"Port            : {configuration['port']}")
    print(f"Run name        : {configuration['name']}")
    print(f"Output directory: {configuration['output_base']}")
    print("-----------------------------\n")

    print("Writing to file: 'config/server_run_cfg.json'")
    with open("config/server_run_cfg.json", "w") as f:
        json.dump(configuration, f, indent=4)


if __name__ == "__main__":
    main()
