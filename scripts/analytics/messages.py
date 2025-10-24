import json
import numpy as np
from scripts.analytics.loader import load_latest_run_cfg
from scripts.analytics.data_streamer import DataStreamer

MESSAGE_TYPES = {
    1: "HELLO",
    2: "HELLO_ACK",
    4: "LOGOUT",
    5: "LOGOUT_ACK",
    10: "NEW_ORDER",
    11: "ORDER_ACK",
    12: "CANCEL_ORDER",
    13: "CANCEL_ACK",
    14: "MODIFY_ORDER",
    15: "MODIFY_ACK",
    20: "TRADE",
}

REQUEST_RESPONSE_PAIRS = {
    1: 2,  # HELLO -> HELLO_ACK
    10: 11,  # NEW_ORDER -> ORDER_ACK
    12: 13,  # CANCEL_ORDER -> CANCEL_ACK
    14: 15,  # MODIFY_ORDER -> MODIFY_ACK
    4: 5,  # LOGOUT -> LOGOUT_ACK
}


def make_metadata(cfg):
    metadata = {}
    metadata["base_tick"] = cfg.get("baseTick", 0)
    metadata["ns_per_tick"] = cfg.get("nsPerTick", 0.5)
    metadata["build_metadata"] = cfg.get("build_metadata", None)
    return metadata


def make_stats(cfg):
    streamer = DataStreamer(cfg, chunk_size=100_000)  # Larger chunks for efficiency
    recv_stream = streamer.stream("recv_messages")
    send_stream = streamer.stream("sent_messages")

    ns_per_tick = cfg.get("nsPerTick", 0.5)

    stats = {
        "metadata": make_metadata(cfg),
        "latencies": [],
        "by_message_type": {},  # Track latencies per message type
        "matched_count": 0,
        "unmatched_count": 0,
        "duplicate_responses": 0,
    }

    recv_lookup = {}
    matched_keys = set()  # Track which keys we've already matched

    for recv_chunk in recv_stream:
        print(f"Processing recv chunk with {len(recv_chunk)} messages")
        send_chunk = next(send_stream, None)

        if send_chunk is None:
            print("No more send chunks")
            break

        for recv_row in recv_chunk:
            msg_type = recv_row["type"]

            if msg_type in REQUEST_RESPONSE_PAIRS:
                recv_key = (recv_row["clientID"], recv_row["ref"], msg_type)
                recv_lookup[recv_key] = recv_row["ts"]

        for send_row in send_chunk:
            send_type = send_row["type"]

            request_type = None
            for req, resp in REQUEST_RESPONSE_PAIRS.items():
                if resp == send_type:
                    request_type = req
                    break

            if request_type is None:
                continue

            send_key = (send_row["clientID"], send_row["ref"], request_type)

            if send_key in recv_lookup:
                if send_key in matched_keys:
                    stats["duplicate_responses"] += 1
                    msg_type_name = MESSAGE_TYPES.get(send_type, f"TYPE_{send_type}")
                    continue

                # Calculate latency
                recv_ts = recv_lookup[send_key]
                send_ts = send_row["ts"]

                latency_ticks = send_ts - recv_ts
                latency_ns = latency_ticks * ns_per_tick

                if latency_ns < 0:
                    print(
                        f" Negative latency: client={send_row['clientID']}, ref={send_row['ref']}"
                    )
                    continue

                stats["latencies"].append(latency_ns)
                stats["matched_count"] += 1
                matched_keys.add(send_key)

                msg_type_name = MESSAGE_TYPES.get(request_type, f"TYPE_{request_type}")
                if msg_type_name not in stats["by_message_type"]:
                    stats["by_message_type"][msg_type_name] = []
                stats["by_message_type"][msg_type_name].append(latency_ns)

            else:
                stats["unmatched_count"] += 1

    if stats["latencies"]:
        latencies_array = np.array(stats["latencies"])

        stats["latency_stats"] = {
            "count": len(stats["latencies"]),
            "min": float(np.min(latencies_array)),
            "max": float(np.max(latencies_array)),
            "mean": float(np.mean(latencies_array)),
            "median": float(np.median(latencies_array)),
            "std_dev": float(np.std(latencies_array)),
            "percentiles": {
                "p50": float(np.percentile(latencies_array, 50)),
                "p75": float(np.percentile(latencies_array, 75)),
                "p90": float(np.percentile(latencies_array, 90)),
                "p95": float(np.percentile(latencies_array, 95)),
                "p99": float(np.percentile(latencies_array, 99)),
                "p99.9": float(np.percentile(latencies_array, 99.9)),
            },
        }

        print("\n" + "=" * 60)
        print("OVERALL LATENCY STATISTICS")
        print("=" * 60)
        ls = stats["latency_stats"]
        print(f"Count: {ls['count']:,}")
        print(f"Mean: {ls['mean']:.2f} ns ({ls['mean']/1000:.2f} µs)")
        print(f"Median: {ls['median']:.2f} ns ({ls['median']/1000:.2f} µs)")
        print(
            f"p95: {ls['percentiles']['p95']:.2f} ns ({ls['percentiles']['p95']/1000:.2f} µs)"
        )
        print(
            f"p99: {ls['percentiles']['p99']:.2f} ns ({ls['percentiles']['p99']/1000:.2f} µs)"
        )

    for msg_type, latencies in stats["by_message_type"].items():
        if latencies:
            lat_array = np.array(latencies)
            print(f"\n{msg_type}:")
            print(f"  Count: {len(latencies):,}")
            print(f"  Mean: {np.mean(lat_array)/1000:.2f} µs")
            print(f"  Median: {np.median(lat_array)/1000:.2f} µs")
            print(f"  p95: {np.percentile(lat_array, 95)/1000:.2f} µs")

    return stats


if __name__ == "__main__":
    cfg = load_latest_run_cfg()
    stats = make_stats(cfg)

    print(f"\n" + "=" * 60)
    print(f"Total matched: {stats['matched_count']}")
    print(f"Total unmatched: {stats['unmatched_count']}")
    print(f"Duplicate responses: {stats['duplicate_responses']}")

    output_stats = {
        k: v for k, v in stats.items() if k not in ["latencies", "by_message_type"]
    }
