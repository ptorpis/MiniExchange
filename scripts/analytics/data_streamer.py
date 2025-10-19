from pathlib import Path
import csv


class DataStreamer:
    def __init__(self, run_config: dict, chunk_size: int = 10000) -> None:
        self.files = run_config.get("files", {})
        self.chunk_size = chunk_size

    def _stream_csv(self, path: Path):
        with path.open(newline="") as f:
            reader = csv.DictReader(f)
            batch = []
            for row in reader:
                batch.append({k: int(v) for k, v in row.items()})  # cast to int
                if len(batch) >= self.chunk_size:
                    yield batch
                    batch = []

            if batch:
                yield batch

    def stream(self, name: str):
        path_str = self.files.get(name)
        if not path_str:
            raise ValueError(f"No CSV file for '{name}' found in run_config.")

        path = Path(path_str)
        if not path.exists():
            raise FileNotFoundError(f"CSV file for {path} does not exist")
        yield from self._stream_csv(path)

    def stream_single_rows(self, name: str):
        for batch in self.stream(name):
            for row in batch:
                yield row
