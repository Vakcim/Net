#!/usr/bin/env python3
"""Run deterministic benchmark repetitions and collect one CSV file."""

from __future__ import annotations

import argparse
import csv
import pathlib
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default="build/temporal_benchmark")
    parser.add_argument("--output", default="results/benchmark.csv")
    parser.add_argument("--seeds", default="1,2,3,4,5")
    parser.add_argument("--vertices", type=int, default=1000)
    parser.add_argument("--events", type=int, default=20000)
    parser.add_argument("--queries", type=int, default=5000)
    parser.add_argument("--verify-queries", type=int, default=100)
    parser.add_argument("--time-buckets", type=int, default=2000)
    parser.add_argument("--gamma", type=float, default=2.5)
    parser.add_argument("--model", default="dag-pareto", choices=["dag-pareto", "random-pareto"])
    parser.add_argument("--thresholds", default="4,8,16,32,64,128,256")
    args = parser.parse_args()

    output = pathlib.Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    seeds = [int(x) for x in args.seeds.split(",") if x]
    thresholds = [
        int(x)
        for x in args.thresholds.split(",")
        if x
    ]

    rows: list[dict[str, str]] = []
    fieldnames: list[str] | None = None

    for seed in seeds:
        for threshold in thresholds:
            command = [
                args.binary,
                "--vertices", str(args.vertices),
                "--events", str(args.events),
                "--queries", str(args.queries),
                "--verify-queries", str(args.verify_queries),
                "--time-buckets", str(args.time_buckets),
                "--gamma", str(args.gamma),
                "--model", args.model,
                "--seed", str(seed),
                "--thresholds", str(threshold),
            ]

            completed = subprocess.run(
                command,
                check=True,
                text=True,
                capture_output=True,
            )

            reader = csv.DictReader(
                completed.stdout.splitlines()
            )
            fieldnames = reader.fieldnames
            rows.extend(reader)
            
    if not fieldnames:
        raise RuntimeError("benchmark produced no CSV header")
    with output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    print(f"wrote {len(rows)} rows to {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
