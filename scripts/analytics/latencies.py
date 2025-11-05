from scripts.analytics.data_streamer import DataStreamer
from scripts.analytics.loader import load_latest_run_cfg
from collections import defaultdict

import csv
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
            "recv_to_deser": [],
            "recv_to_buf": [],
            "deser_to_buf": [],
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

            stats[msg_type]["recv_to_deser"].append(recv_to_deser)
            stats[msg_type]["recv_to_buf"].append(recv_to_buf)
            stats[msg_type]["deser_to_buf"].append(deser_to_buf)

    for msg_type, s in stats.items():
        count = len(s["recv_to_deser"])
        print(f"\n{'='*70}")
        print(f"Message type {MESSAGE_TYPES[msg_type]} ({count:,} samples)")
        print(f"{'='*70}")

        recv_deser = np.array(s["recv_to_deser"])
        deser_buf = np.array(s["deser_to_buf"])
        total = np.array(s["recv_to_buf"])

        percentiles = [1, 5, 10, 25, 50, 75, 90, 95, 99, 99.9, 99.99]

        print("\nReceive -> Deserialize:")
        print(f"  Mean:   {np.mean(recv_deser):>8.1f} ns")
        print(f"  Median: {np.median(recv_deser):>8.1f} ns")
        print(f"  Std:    {np.std(recv_deser):>8.1f} ns")
        print(f"  Min:    {np.min(recv_deser):>8.1f} ns")
        print(f"  Max:    {np.max(recv_deser):>8.1f} ns")
        print("\n  Percentiles:")
        for p in percentiles:
            val = np.percentile(recv_deser, p)
            print(f"    p{p:>5}: {val:>8.1f} ns ({val/1000:>6.2f} µs)")

        print("\nDeserialize -> Buffer:")
        print(f"  Mean:   {np.mean(deser_buf):>8.1f} ns")
        print(f"  Median: {np.median(deser_buf):>8.1f} ns")
        print(f"  Std:    {np.std(deser_buf):>8.1f} ns")
        print(f"  Min:    {np.min(deser_buf):>8.1f} ns")
        print(f"  Max:    {np.max(deser_buf):>8.1f} ns")
        print("\n  Percentiles:")
        for p in percentiles:
            val = np.percentile(deser_buf, p)
            print(f"    p{p:>5}: {val:>8.1f} ns ({val/1000:>6.2f} µs)")

        print("\nTotal (Receive -> Buffer):")
        print(f"  Mean:   {np.mean(total):>8.1f} ns")
        print(f"  Median: {np.median(total):>8.1f} ns")
        print(f"  Std:    {np.std(total):>8.1f} ns")
        print(f"  Min:    {np.min(total):>8.1f} ns")
        print(f"  Max:    {np.max(total):>8.1f} ns")
        print("\n  Percentiles:")
        for p in percentiles:
            val = np.percentile(total, p)
            print(f"    p{p:>5}: {val:>8.1f} ns ({val/1000:>6.2f} µs)")


def print_summary_and_export_csv(output_file="latency_percentiles.csv"):
    """Print summary to console AND export detailed percentiles to CSV."""
    run_cfg = load_latest_run_cfg()
    base_tick = run_cfg.get("baseTick", 0)
    ns_per_tick = run_cfg.get("nsPerTick", None)

    stats = defaultdict(
        lambda: {
            "recv_to_deser": [],
            "deser_to_buf": [],
            "recv_to_buf": [],
        }
    )

    print("Loading timing data...")
    streamer = DataStreamer(run_cfg)
    for chunk in streamer.stream("timings"):
        for row in chunk:
            msg_type = row["messageType"]
            recv = (row["tReceived"] - base_tick) * ns_per_tick
            deser = (row["tDeserialized"] - base_tick) * ns_per_tick
            buf = (row["tBuffered"] - base_tick) * ns_per_tick

            stats[msg_type]["recv_to_deser"].append(deser - recv)
            stats[msg_type]["deser_to_buf"].append(buf - deser)
            stats[msg_type]["recv_to_buf"].append(buf - recv)

    print("\n" + "=" * 70)
    print("INTERNAL LATENCY SUMMARY")
    print("=" * 70)

    for msg_type, data in sorted(stats.items()):
        msg_name = MESSAGE_TYPES.get(msg_type, f"TYPE_{msg_type}")
        count = len(data["recv_to_buf"])

        r2d = np.array(data["recv_to_deser"])
        d2b = np.array(data["deser_to_buf"])
        total = np.array(data["recv_to_buf"])

        print(f"\nMessage type {msg_name} ({count:,} samples):")
        print(f"  recv->deser mean: {np.mean(r2d):.1f} ns (max {np.max(r2d):.1f})")
        print(f"  deser->buf  mean: {np.mean(d2b):.1f} ns (max {np.max(d2b):.1f})")
        print(f"  recv->buf   mean: {np.mean(total):.1f} ns (max {np.max(total):.1f})")

    print(f"\n{'='*70}")
    print(f"Writing detailed percentiles to {output_file}...")
    print("=" * 70)

    with open(output_file, "w", newline="") as f:
        writer = csv.writer(f)

        writer.writerow(
            [
                "message_type",
                "samples",
                "metric",
                "percentile",
                "recv_to_deser_ns",
                "recv_to_deser_us",
                "deser_to_buf_ns",
                "deser_to_buf_us",
                "total_ns",
                "total_us",
            ]
        )

        percentiles = [1, 5, 10, 25, 50, 75, 90, 95, 99, 99.9, 99.99]

        for msg_type, data in sorted(stats.items()):
            msg_name = MESSAGE_TYPES.get(msg_type, f"TYPE_{msg_type}")
            sample_count = len(data["recv_to_buf"])

            for stat_name, stat_func in [
                ("mean", np.mean),
                ("median", np.median),
                ("std", np.std),
                ("min", np.min),
                ("max", np.max),
            ]:
                r2d = stat_func(data["recv_to_deser"])
                d2b = stat_func(data["deser_to_buf"])
                total = stat_func(data["recv_to_buf"])

                writer.writerow(
                    [
                        msg_name,
                        sample_count,
                        stat_name,
                        "",
                        f"{r2d:.1f}",
                        f"{r2d/1000:.3f}",
                        f"{d2b:.1f}",
                        f"{d2b/1000:.3f}",
                        f"{total:.1f}",
                        f"{total/1000:.3f}",
                    ]
                )

            for p in percentiles:
                r2d = np.percentile(data["recv_to_deser"], p)
                d2b = np.percentile(data["deser_to_buf"], p)
                total = np.percentile(data["recv_to_buf"], p)

                writer.writerow(
                    [
                        msg_name,
                        sample_count,
                        "percentile",
                        f"p{p}",
                        f"{r2d:.1f}",
                        f"{r2d/1000:.3f}",
                        f"{d2b:.1f}",
                        f"{d2b/1000:.3f}",
                        f"{total:.1f}",
                        f"{total/1000:.3f}",
                    ]
                )

    print(f"Exported to {output_file}")

    print("\n" + "=" * 60)
    print("PERCENTILE SUMMARY (Total Latency)")
    print("=" * 106)
    print(
        f"{'Message Type':<15} {'Samples':>9} {'p10':>9} {'p25':>9} {'p50':>9} {'p75':>9} {'p90':>9} {'p95':>9} {'p99':>9} {'p99.9':>9}"
    )
    print("-" * 106)

    for msg_type, data in sorted(stats.items()):
        msg_name = MESSAGE_TYPES.get(msg_type, f"TYPE_{msg_type}")
        count = len(data["recv_to_buf"])
        p10 = np.percentile(data["recv_to_buf"], 10) / 1000
        p25 = np.percentile(data["recv_to_buf"], 25) / 1000
        p50 = np.percentile(data["recv_to_buf"], 50) / 1000
        p75 = np.percentile(data["recv_to_buf"], 75) / 1000
        p90 = np.percentile(data["recv_to_buf"], 90) / 1000
        p95 = np.percentile(data["recv_to_buf"], 95) / 1000
        p99 = np.percentile(data["recv_to_buf"], 99) / 1000
        p999 = np.percentile(data["recv_to_buf"], 99.9) / 1000

        print(
            f"{msg_name:<15} {count:>10,} {p10:>7.2f}µs {p25:>7.2f}µs {p50:>7.2f}µs {p75:>7.2f}µs {p90:>7.2f}µs {p95:>7.2f}µs {p99:>7.2f}µs {p999:>7.2f}µs"
        )

    print("=" * 106)


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

            for i, val in enumerate((recv_to_deser, deser_to_buf)):
                idx = np.searchsorted(bin_edges, val, side="right") - 1
                if 0 <= idx < len(bin_edges) - 1:
                    histograms[msg_type]["breakdown"][i, idx] += 1

            idx = np.searchsorted(bin_edges, total_latency, side="right") - 1
            if 0 <= idx < len(bin_edges) - 1:
                histograms[msg_type]["total"][idx] += 1

    return histograms, bin_edges


def _format_ns_label(v: float) -> str:
    """Format a tick value (nanoseconds) into a human-friendly label."""
    if v < 1_000:
        return f"{int(v)} ns"
    if v < 1_000_000:
        us = v / 1_000.0
        return f"{us:.0f} µs" if us >= 10 else f"{us:.1f} µs"
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
            edgecolor="none",
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
            edgecolor="none",
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
            label="Total (Receive -> Response Buffered)",
            align="edge",
            edgecolor="none",
            linewidth=0.5,
            color=colors["total"],
        )

        ax2.set_xscale("log")
        ax2.set_xlim(x_min, x_max)
        ax2.set_xlabel("Latency")
        ax2.set_ylabel("Count")
        ax2.set_title(
            f"Total Internal Latency - Message Type: {MESSAGE_TYPES[msg_type]}"
        )
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
    # print_summary()
    print_summary_and_export_csv()
    streamer = DataStreamer(run_config=load_latest_run_cfg())
    histograms, bins = analyze_latencies_hist(streamer, load_latest_run_cfg())
    plot_latency_histograms(histograms, bins)
