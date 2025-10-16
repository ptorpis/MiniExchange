import pandas as pd
import argparse
import os

def main():
    parser = argparse.ArgumentParser(
        description="Summarize CPU cycle timestamp timings from a CSV file."
    )
    parser.add_argument(
        "file_path",
        help="Path to the CSV file containing the timings."
    )
    parser.add_argument(
        "--nsptick",
        type=float,
        default=0.527423,
        help="Nanoseconds per CPU tick (default: 0.527423)."
    )
    parser.add_argument(
        "--out",
        help="Optional output CSV file to save the summary."
    )

    args = parser.parse_args()

    df = pd.read_csv(args.file_path)

    pd.set_option('display.float_format', '{:.2f}'.format)
    pd.set_option('display.max_columns', None)

    df['tReceived_ns'] = df['tReceived'] * args.nsptick
    df['tDeserialized_ns'] = df['tDeserialized'] * args.nsptick
    df['tBuffered_ns'] = df['tBuffered'] * args.nsptick

    df['deserialize_latency_ns'] = df['tDeserialized_ns'] - df['tReceived_ns']
    df['buffer_latency_ns'] = df['tBuffered_ns'] - df['tDeserialized_ns']
    df['total_latency_ns'] = df['tBuffered_ns'] - df['tReceived_ns']

    summary = df.groupby('messageType').agg(
        count=('messageType', 'size'),
        avg_deserialize_ns=('deserialize_latency_ns', 'mean'),
        max_deserialize_ns=('deserialize_latency_ns', 'max'),
        avg_buffer_ns=('buffer_latency_ns', 'mean'),
        max_buffer_ns=('buffer_latency_ns', 'max'),
        avg_total_ns=('total_latency_ns', 'mean'),
        max_total_ns=('total_latency_ns', 'max'),
    ).reset_index()


    if args.out:
        if not args.out.lower().endswith(".csv"):
            print(f"Warning: output file '{args.out}' does not have .csv extension. Adding .csv automatically.")
            args.out += ".csv"

        out_dir = os.path.dirname(args.out)
        if out_dir and not os.path.exists(out_dir):
            try:
                os.makedirs(out_dir)
                print(f"Created directory {out_dir}")
            except Exception as e:
                print(f"Error creating directory {out_dir}: {e}")
                exit(1)

        try:
            summary.to_csv(args.out, index=False)
            print(f"Summary saved to {args.out}")
        except Exception as e:
            print(f"Error saving CSV: {e}")
            exit(1)
    else:
        print(summary)

if __name__ == "__main__":
    main()
