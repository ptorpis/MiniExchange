import pandas as pd
from pathlib import Path
from typing import Iterator
from py.analytics.config import get_output_dir

def iter_event_file(file_path: Path, chunksize: int = 100_000) -> Iterator[pd.DataFrame]:
    """Iterate over a large CSV file in chunks."""
    for chunk in pd.read_csv(file_path, chunksize=chunksize):
        yield chunk

def load_all_events(chunksize: int = 100_000):
    """
    Yield (event_type, chunk_df) pairs for all CSV files in the output directory.
    The event_type is derived from the filename without extension.
    """
    base_dir = get_output_dir()
    for file_path in sorted(base_dir.glob("*.csv")):
        event_type = file_path.stem
        for chunk in iter_event_file(file_path, chunksize):
            yield event_type, chunk
