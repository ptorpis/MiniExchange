import tests.bulk_testing.csv_testing as cvt
import tests.bulk_testing.generate_fuzz as gf
from src.feeds.events import EventBus
from src.auth.session_manager import SessionManager, VSD
from src.api.api import ExchangeAPI


if __name__ == "__main__":
    num_of_rows = [10_000, 100_000, 1_000_000, 10_000_000]
    num_of_runs = 10
    names = ["10k", "100k", "1m", "10m",]

    for i in range(len(num_of_rows)):
        for run in range(num_of_runs):
            file_path = (
                    f"tests/bulk_testing/csvs/benchmark/"
                    f"fuzz_test_{run + 1}_{names[i]}.csv")
            event_bus = EventBus(test_mode=True)
            sm = SessionManager(user_db=VSD)
            api = ExchangeAPI(event_bus=event_bus, session_manager=sm)
            gf.main(file_path=file_path, num_lines=num_of_rows[i])
            results = cvt.main(
                file_path=file_path,
                api=api, sm=sm)
            event_bus.shutdown()

            cvt.export_benchmark_csv(
                "tests/bulk_testing/csvs/benchmark_results5.csv",
                f"fuzz_test_#{run + 1}_{names[i]}",
                *results
            )
            print(f"Completed run: #{run + 1} with {num_of_rows[i]} rows.")
