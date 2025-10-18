import json
import argparse
import subprocess
from pathlib import Path

DEFAULTS = {
    "build_type": "Debug",
    "enable_logging": True,
    "enable_timing": True,
    "enable_asan": False,
    "enable_lto": False,
}

BUILD_CHOICES = ["Debug", "Release", "RelWithDebInfo"]


class FlagProvidedAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        setattr(namespace, self.dest, values)
        setattr(namespace, f"{self.dest}_provided", True)


def prompt_choice(prompt, choices, default):
    print(f"{prompt} (default={default})")
    for i, c in enumerate(choices, 1):
        print(f" {i}) {c}")
    selection = input("Choice: ").strip()
    if not selection:
        return default
    try:
        idx = int(selection) - 1
        if 0 <= idx < len(choices):
            return choices[idx]
    except:
        pass
    if selection in choices:
        return selection
    print("Invalid choice, using default.")
    return default


def prompt_bool(prompt, default):
    val = input(f"{prompt} [y/n] (default={'y' if default else 'n'}): ").strip().lower()
    if not val:
        return default
    return val in ["y", "yes"]


def main():
    parser = argparse.ArgumentParser(description="Build MiniExchange with options.")
    parser.add_argument(
        "--interactive",
        "-i",
        action="store_true",
        help="Configure build interactively.",
    )
    parser.add_argument(
        "--build-type",
        "-b",
        choices=BUILD_CHOICES,
        action=FlagProvidedAction,
        default=DEFAULTS["build_type"],
    )
    parser.add_argument(
        "--enable-logging",
        action=FlagProvidedAction,
        type=bool,
        default=DEFAULTS["enable_logging"],
        help="Enable event logging",
    )
    parser.add_argument(
        "--enable-timing",
        action=FlagProvidedAction,
        type=bool,
        default=DEFAULTS["enable_timing"],
        help="Enable high-resolution timing",
    )
    parser.add_argument(
        "--enable-asan",
        action=FlagProvidedAction,
        type=bool,
        default=DEFAULTS["enable_asan"],
        help="Enable AddressSanitizer",
    )
    parser.add_argument(
        "--enable-lto",
        action=FlagProvidedAction,
        type=bool,
        default=DEFAULTS["enable_lto"],
        help="Enable Link Time Optimization (LTO)",
    )

    args = parser.parse_args()

    if args.interactive:
        print("\n--- Interactive Build Configuration ---\n")
        build_type = (
            args.build_type
            if getattr(args, "build_type_provided", False)
            else prompt_choice(
                "Select build type", BUILD_CHOICES, DEFAULTS["build_type"]
            )
        )
        enable_logging = (
            args.enable_logging
            if getattr(args, "enable_logging_provided", False)
            else prompt_bool("Enable event logging?", DEFAULTS["enable_logging"])
        )
        enable_timing = (
            args.enable_timing
            if getattr(args, "enable_timing_provided", False)
            else prompt_bool("Enable timing?", DEFAULTS["enable_timing"])
        )
        enable_asan = (
            args.enable_asan
            if getattr(args, "enable_asan_provided", False)
            else prompt_bool("Enable AddressSanitizer?", DEFAULTS["enable_asan"])
        )
        enable_lto = (
            args.enable_lto
            if getattr(args, "enable_lto_provided", False)
            else prompt_bool("Enable LTO?", DEFAULTS["enable_lto"])
        )
        print("\nConfiguration collected from user input.\n")
    else:
        print("\n--- Auto Build Mode ---\n")
        build_type = args.build_type
        enable_logging = args.enable_logging
        enable_timing = args.enable_timing
        enable_asan = args.enable_asan
        enable_lto = args.enable_lto
        print("Using provided or default values.\n")

    build_dir = f"build-{build_type.lower()}"
    cmake_cmd = [
        "cmake",
        "-S",
        ".",
        "-B",
        build_dir,
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DENABLE_LOGGING={'ON' if enable_logging else 'OFF'}",
        f"-DENABLE_TIMING={'ON' if enable_timing else 'OFF'}",
        f"-DENABLE_ASAN={'ON' if enable_asan else 'OFF'}",
        f"-DENABLE_LTO={'ON' if enable_lto else 'OFF'}",
    ]

    print("Running CMake configure...")
    subprocess.check_call(cmake_cmd)

    print("Building...")
    subprocess.check_call(["cmake", "--build", build_dir, "-j"])

    config_path = Path("config/build_config.json")

    config_path.parent.mkdir(parents=True, exist_ok=True)

    Path(config_path).write_text(
        json.dumps(
            {
                "build_type": build_type,
                "enable_logging": enable_logging,
                "enable_timing": enable_timing,
                "enable_asan": enable_asan,
                "enable_lto": enable_lto,
                "build_dir": build_dir,
            },
            indent=4,
        )
    )

    print(f"Build finished! Directory: {build_dir}")
    print("Config saved to config/build_config.json for preprocessing.")


if __name__ == "__main__":
    main()
