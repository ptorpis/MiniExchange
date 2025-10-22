import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict, Counter
from pathlib import Path
import json

from scripts.analytics.data_streamer import DataStreamer
from scripts.analytics.loader import load_latest_run_cfg


class MessageAnalytics:
    """Analytics for received message events"""

    # Message type mappings
    MESSAGE_TYPES = {
        1: "HELLO",
        2: "LOGOUT",
        10: "NEW_ORDER",
        12: "CANCEL_ORDER",
        14: "MODIFY_ORDER",
        3: "HEARTBEAT",
    }

    def __init__(self, run_config: dict):
        self.run_config = run_config
        self.streamer = DataStreamer(run_config, chunk_size=50000)
        self.ns_per_tick = run_config.get("nsPerTick", 0.5)

    def compute_message_counts(self):
        """Count messages by type"""
        counts = Counter()

        for batch in self.streamer.stream("recv_messages"):
            for row in batch:
                msg_type = row["type"]
                counts[msg_type] += 1

        return counts

    def compute_messages_per_second(self):
        """Calculate throughput over time"""
        # Group messages by second
        messages_by_second = defaultdict(int)

        for batch in self.streamer.stream("recv_messages"):
            for row in batch:
                ts_ticks = row["ts"]
                ts_ns = ts_ticks * self.ns_per_tick
                second = int(ts_ns // 1_000_000_000)  # Convert ns to seconds
                messages_by_second[second] += 1

        # Convert to sorted lists
        seconds = sorted(messages_by_second.keys())
        if not seconds:
            return [], []

        # Normalize to start at 0
        start_second = seconds[0]
        relative_seconds = [s - start_second for s in seconds]
        throughput = [messages_by_second[s] for s in seconds]

        return relative_seconds, throughput

    def compute_client_activity(self):
        """Messages per client"""
        client_counts = Counter()

        for batch in self.streamer.stream("recv_messages"):
            for row in batch:
                client_id = row["clientID"]
                client_counts[client_id] += 1

        return client_counts

    def compute_message_type_timeline(self):
        """Message types over time"""
        timeline = defaultdict(lambda: defaultdict(int))

        for batch in self.streamer.stream("recv_messages"):
            for row in batch:
                ts_ticks = row["ts"]
                ts_ns = ts_ticks * self.ns_per_tick
                second = int(ts_ns // 1_000_000_000)
                msg_type = row["type"]
                timeline[second][msg_type] += 1

        return timeline

    def compute_interarrival_times(self, sample_size=10000):
        """Calculate message interarrival times (sampled)"""
        interarrivals = []
        prev_ts = None
        count = 0

        for row in self.streamer.stream_single_rows("recv_messages"):
            ts_ticks = row["ts"]
            ts_ns = ts_ticks * self.ns_per_tick

            if prev_ts is not None:
                interarrivals.append(ts_ns - prev_ts)
                count += 1
                if count >= sample_size:
                    break
            prev_ts = ts_ns

        return interarrivals

    def generate_summary_report(self):
        """Generate text summary"""
        print("=" * 60)
        print("MESSAGE ANALYTICS SUMMARY")
        print("=" * 60)

        # Message counts
        counts = self.compute_message_counts()
        total = sum(counts.values())

        print(f"\nTotal Messages: {total:,}")
        print("\nMessage Type Breakdown:")
        for msg_type, count in sorted(counts.items()):
            type_name = self.MESSAGE_TYPES.get(msg_type, f"UNKNOWN_{msg_type}")
            pct = (count / total) * 100
            print(f"  {type_name:15s}: {count:8,} ({pct:5.2f}%)")

        seconds, throughput = self.compute_messages_per_second()
        if throughput:
            print(f"\nThroughput Statistics:")
            print(f"  Average: {np.mean(throughput):,.0f} msg/sec")
            print(f"  Peak:    {np.max(throughput):,.0f} msg/sec")
            print(f"  Min:     {np.min(throughput):,.0f} msg/sec")
            print(f"  Std Dev: {np.std(throughput):,.0f} msg/sec")

        client_counts = self.compute_client_activity()
        if client_counts:
            print(f"\nClient Statistics:")
            print(f"  Total Clients: {len(client_counts)}")
            print(f"  Avg msgs/client: {np.mean(list(client_counts.values())):,.0f}")
            print(
                f"  Most active: Client {client_counts.most_common(1)[0][0]} "
                f"({client_counts.most_common(1)[0][1]:,} messages)"
            )

        interarrivals = self.compute_interarrival_times(sample_size=10000)
        if interarrivals:
            interarrivals_us = [x / 1000 for x in interarrivals]  # ns to µs
            print(f"\nInterarrival Times (sample of {len(interarrivals):,}):")
            print(f"  Mean:   {np.mean(interarrivals_us):.2f} µs")
            print(f"  Median: {np.median(interarrivals_us):.2f} µs")
            print(f"  p95:    {np.percentile(interarrivals_us, 95):.2f} µs")
            print(f"  p99:    {np.percentile(interarrivals_us, 99):.2f} µs")

        print("=" * 60)


def main():
    """Main entry point"""
    run_config = load_latest_run_cfg()

    print(f"Analyzing run: {run_config.get('run_id', 'unknown')}")
    print(f"Start time: {run_config.get('start_time', 'unknown')}\n")

    analytics = MessageAnalytics(run_config)

    analytics.generate_summary_report()


if __name__ == "__main__":
    main()
