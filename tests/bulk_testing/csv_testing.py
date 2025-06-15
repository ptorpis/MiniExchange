import csv
import time
from datetime import datetime, UTC

from src.auth.session_manager import SessionManager, VSD
from src.api.api import ExchangeAPI
from src.feeds.events import EventBus


def stream_csv_requests(file_path: str, sm: SessionManager) -> dict:
    with open(file_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            request = decode_csv_row(row, sm)
            if request:
                yield request


def decode_csv_row(row: dict, sm: SessionManager):
    request_type = row["action"]
    username = row["username"]

    match request_type:
        case "login":
            return {
                "type": "login",
                "payload": {
                    "username": username,
                    "password": "test"
                }
            }

        case "logout":
            token = sm.get_token(username)
            if not token:
                return None
            return {
                "type": "logout",
                "payload": {
                    "token": token
                }
            }

        case "order":
            token = sm.get_token(username)
            if not token:
                return None

            try:
                payload = {
                    "token": token,
                    "side": row["side"],
                    "qty": float(row["qty"]),
                    "order_type": row["order_type"]
                }
                if row["order_type"] == "limit":
                    payload["price"] = float(row["price"])
                return {"type": "order", "payload": payload}
            except (ValueError, KeyError):
                return None

        case _:
            return None


def export_benchmark_csv(
        output_file: str,
        test_name: str,
        total_requests: int,
        faults: int,
        elapsed_time: float
):
    with open(output_file, mode='a', newline='') as csvfile:
        writer = csv.writer(csvfile)

        if csvfile.tell() == 0:
            writer.writerow(
                ["timestamp",
                 "test_name",
                 "total_requests",
                 "faults",
                 "elapsed_time_s",
                 "req_per_sec"])

        writer.writerow([
            datetime.now(UTC).isoformat(),
            test_name,
            total_requests,
            faults,
            round(elapsed_time, 4),
            round(total_requests / elapsed_time, 2)
        ])


def main(file_path: str, api: ExchangeAPI, sm: SessionManager):
    total_requests = 0
    fault = 0

    start_time = time.perf_counter()

    for request in stream_csv_requests(file_path, sm):
        response = api.handle_request(request)
        if response["success"] is False:
            fault += 1
        total_requests += 1

    end_time = time.perf_counter()

    elapsed_time = end_time - start_time
    return total_requests, fault, elapsed_time


if __name__ == "__main__":
    file_path = "tests/bulk_testing/csvs/fuzz_test.csv"

    event_bus = EventBus(test_mode=True)
    sm = SessionManager(user_db=VSD)
    api = ExchangeAPI(event_bus=event_bus, session_manager=sm)

    results = main(
        file_path=file_path,
        api=api, sm=sm)

    export_benchmark_csv(
        "tests/bulk_testing/csvs/benchmark_results.csv",
        "fuzz_test_1m",
        *results
    )
    print(f"Processed {results[0]} requests "
          f"in {results[2]:.4f} seconds.")
    print(f"Faulty requests: {results[1]}")
    print(f"Requests per second: "
          f"{results[0] / results[2]:.2f}")
