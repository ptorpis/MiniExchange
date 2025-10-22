from scripts.analytics.data_streamer import DataStreamer
from scripts.analytics.loader import load_latest_run_cfg
from collections import defaultdict

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import LogLocator, NullFormatter

LATENCY_LABELS = [
    "Received Message -> Deserialized Message",
    "Deserialized Message -> Message (response) Buffered",
]

MESSAGE_TYPES = {
    1: "HELLO",
    2: "HELLO_ACK",
    3: "HEARTBEAT",
    10: "NEW_ORDER",
    12: "CANCEL_ORDER",
    14: "MODIFY_ORDER",
}

colors = {
    "recv -> deser": "#4C72B0",  # muted blue
    "deser -> buf": "#DD8452",  # muted orange
    "total": "#55A868",  # green for total
}


def print_summary():
    run_cfg = load_latest_run_cfg()
    base_tick = run_cfg.get("baseTick", 0)
    ns_per_tick = run_cfg.get("nsPerTick", None)

    stats = defaultdict(
        lambda: {
            "count": 0,
            "sum_recv_to_deser": 0,
            "sum_recv_to_buf": 0,
            "sum_deser_to_buf": 0,
            "max_recv_to_deser": 0,
            "max_recv_to_buf": 0,
            "max_deser_to_buf": 0,
        }
    )

    streamer = DataStreamer(run_cfg)
    for chunk in streamer.stream("timings"):
        for row in chunk:
            msg_type = row["messageType"]
            recv = (row["tReceived"] - base_tick) * ns_per_tick
            deser = (row["tDeserialized"] - base_tick) * ns_per_tick
            buf = (row["tBuffered"] - base_tick) * ns_per_tick

            recv_to_deser = deser - recv
            recv_to_buf = buf - recv
            deser_to_buf = buf - deser

            s = stats[msg_type]

            s["count"] += 1
            s["sum_recv_to_deser"] += recv_to_deser
            s["sum_recv_to_buf"] += recv_to_buf
            s["sum_deser_to_buf"] += deser_to_buf
            s["max_recv_to_deser"] = max(s["max_recv_to_deser"], recv_to_deser)
            s["max_recv_to_buf"] = max(s["max_recv_to_buf"], recv_to_buf)
            s["max_deser_to_buf"] = max(s["max_deser_to_buf"], deser_to_buf)

    for msg_type, s in stats.items():
        count = s["count"]
        print(f"\nMessage type {MESSAGE_TYPES[msg_type]} ({count} samples):")
        print(
            f"  recv->deser mean: {s['sum_recv_to_deser'] / count:.1f} ns (max {s['max_recv_to_deser']:.1f})"
        )
        print(
            f"  deser->buf  mean: {s['sum_deser_to_buf'] / count:.1f} ns (max {s['max_deser_to_buf']:.1f})"
        )
        print(
            f"  recv->buf   mean: {s['sum_recv_to_buf'] / count:.1f} ns (max {s['max_recv_to_buf']:.1f})"
        )


def analyze_latencies_hist(streamer, run_cfg, num_bins=1000):
    ns_per_tick = run_cfg["nsPerTick"]
    base_tick = run_cfg.get("baseTick", 0)

    bin_edges = np.geomspace(1, 1e6, num_bins)
    histograms = defaultdict(
        lambda: {
            "breakdown": np.zeros((2, len(bin_edges) - 1), dtype=int),
            "total": np.zeros(len(bin_edges) - 1, dtype=int),
        }
    )

    for chunk in streamer.stream("timings"):
        for row in chunk:
            msg_type = row["messageType"]
            recv = (row["tReceived"] - base_tick) * ns_per_tick
            deser = (row["tDeserialized"] - base_tick) * ns_per_tick
            buf = (row["tBuffered"] - base_tick) * ns_per_tick

            recv_to_deser = deser - recv
            deser_to_buf = buf - deser
            total_latency = buf - recv

            # Store breakdown components
            for i, val in enumerate((recv_to_deser, deser_to_buf)):
                idx = np.searchsorted(bin_edges, val, side="right") - 1
                if 0 <= idx < len(bin_edges) - 1:
                    histograms[msg_type]["breakdown"][i, idx] += 1

            # Store total latency
            idx = np.searchsorted(bin_edges, total_latency, side="right") - 1
            if 0 <= idx < len(bin_edges) - 1:
                histograms[msg_type]["total"][idx] += 1

    return histograms, bin_edges


def _format_ns_label(v: float) -> str:
    """Format a tick value (nanoseconds) into a human-friendly label."""
    if v < 1_000:
        return f"{int(v)} ns"
    if v < 1_000_000:
        # microseconds
        us = v / 1_000.0
        # if it's integer-like, show no decimals
        return f"{us:.0f} µs" if us >= 10 else f"{us:.1f} µs"
    # milliseconds and above
    ms = v / 1_000_000.0
    return f"{ms:.2f} ms" if ms < 10 else f"{ms:.1f} ms"


def _log_ticks_for_range(x_min: float, x_max: float, base: int = 10):
    """Return tick positions (1,2,5 * 10**n) within x_min..x_max."""
    if x_min <= 0:
        x_min = 1.0
    lo = int(np.floor(np.log10(x_min)))
    hi = int(np.ceil(np.log10(x_max)))
    multipliers = [1, 2, 5]
    ticks = []
    for exp in range(lo - 1, hi + 1):
        for m in multipliers:
            val = m * (base**exp)
            if x_min <= val <= x_max:
                ticks.append(val)
    # ensure endpoints are included
    ticks = sorted(set(ticks))
    return ticks


def plot_latency_histograms(
    histograms, bins, lower_pct=0.1, upper_pct=99.9, margin_factor=1.1
):
    for msg_type, hist_data in histograms.items():
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))

        bin_centers = (bins[:-1] + bins[1:]) / 2
        bin_widths = np.diff(bins)

        breakdown_latencies = []
        for i in range(2):
            breakdown_latencies.extend(
                np.repeat(bin_centers, hist_data["breakdown"][i])
            )
        breakdown_latencies = np.array(breakdown_latencies)

        total_latencies = np.repeat(bin_centers, hist_data["total"])

        all_latencies = (
            np.concatenate([breakdown_latencies, total_latencies])
            if len(breakdown_latencies) > 0 and len(total_latencies) > 0
            else np.array([1, 1e6])
        )

        if len(all_latencies) > 0:
            lower = np.percentile(all_latencies, lower_pct)
            upper = np.percentile(all_latencies, upper_pct)
            x_min = max(1, lower / margin_factor)
            x_max = upper * margin_factor
        else:
            x_min, x_max = 1, 1e6

        ax1.bar(
            bins[:-1],
            hist_data["breakdown"][0],
            width=bin_widths,
            alpha=0.7,
            label=LATENCY_LABELS[0],
            align="edge",
            edgecolor="black",
            linewidth=0.5,
            color=colors["deser -> buf"],
        )
        ax1.bar(
            bins[:-1],
            hist_data["breakdown"][1],
            width=bin_widths,
            bottom=hist_data["breakdown"][0],
            alpha=0.7,
            label=LATENCY_LABELS[1],
            align="edge",
            edgecolor="black",
            linewidth=0.5,
            color=colors["recv -> deser"],
        )

        ax1.set_xscale("log")
        ax1.set_xlim(x_min, x_max)
        ax1.set_xlabel("Latency")
        ax1.set_ylabel("Count")
        ax1.set_title(f"Latency Breakdown - Message Type: {MESSAGE_TYPES[msg_type]}")
        ax1.legend()
        ax1.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.3)

        ax2.bar(
            bins[:-1],
            hist_data["total"],
            width=bin_widths,
            alpha=0.7,
            label="Total (Receive - Response Buffered)",
            align="edge",
            edgecolor="black",
            linewidth=0.5,
            color=colors["total"],
        )

        ax2.set_xscale("log")
        ax2.set_xlim(x_min, x_max)
        ax2.set_xlabel("Latency")
        ax2.set_ylabel("Count")
        ax2.set_title(f"Total Latency - Message Type: {MESSAGE_TYPES[msg_type]}")
        ax2.legend()
        ax2.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.3)

        ticks = _log_ticks_for_range(float(x_min), float(x_max), base=10)
        if len(ticks) > 0:
            labels = [_format_ns_label(t) for t in ticks]
            ax1.set_xticks(ticks)
            ax1.set_xticklabels(labels, rotation=0, ha="center", fontsize=9)
            ax2.set_xticks(ticks)
            ax2.set_xticklabels(labels, rotation=0, ha="center", fontsize=9)
        else:
            ax1.xaxis.set_major_locator(LogLocator(base=10.0, numticks=12))
            ax2.xaxis.set_major_locator(LogLocator(base=10.0, numticks=12))

        plt.tight_layout()
        plt.show()


if __name__ == "__main__":
    print_summary()
    streamer = DataStreamer(run_config=load_latest_run_cfg())
    histograms, bins = analyze_latencies_hist(streamer, load_latest_run_cfg())
    plot_latency_histograms(histograms, bins)
