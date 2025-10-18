from pathlib import Path
import json
from scripts.analytics.data_streamer import DataStreamer

BASE = Path("output/runs")


def locate_runs():
    configs = []
    for dir in BASE.iterdir():
        cfg = dir / "run_config.json"
        if cfg.exists():
            configs.append(cfg)

    return configs


def latest_run(configs):
    configs.sort(key=lambda p: p.stat().st_mtime)
    return configs[-1]


def load_run_cfg(path):
    with open(path, "r") as file:
        cfg = json.load(file)

    return cfg


if __name__ == "__main__":
    configs = locate_runs()
    latest_conf = latest_run(configs)
    print(latest_conf)
    run_conf = load_run_cfg(latest_conf)
    print(json.dumps(run_conf, indent=4))

    streamer = DataStreamer(run_conf, chunk_size=100)

    total_vol = 0
    for chunk in streamer.stream("trades"):
        for row in chunk:
            total_vol += row["qty"]

    print(total_vol)
