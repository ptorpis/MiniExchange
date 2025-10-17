from pathlib import Path
from dotenv import load_dotenv
import os

def get_output_dir() -> Path:
    """Return the output directory from .env or default fallback."""
    env_path = Path(__file__).resolve().parents[2] / ".env"
    if env_path.exists():
        load_dotenv(env_path)
    output_dir = os.getenv("EXCHANGE_OUTPUT_DIR", "output/default")
    path = Path(output_dir).resolve()
    if not path.exists():
        raise FileNotFoundError(f"Output directory not found: {path}")
    return path
