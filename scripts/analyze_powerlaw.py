#!/usr/bin/env python3
"""Fit the direct promotion law p_L(B) ~ C B^{-kappa} from benchmark CSV.

The script uses only the Python standard library. It aggregates repetitions by
threshold, keeps the non-saturated scaling range, and reports an OLS slope.
"""

from __future__ import annotations

import argparse
import csv
import math
import statistics
from collections import defaultdict


def ols(xs: list[float], ys: list[float]) -> tuple[float, float, float]:
    x_bar = statistics.fmean(xs)
    y_bar = statistics.fmean(ys)
    sxx = sum((x - x_bar) ** 2 for x in xs)
    if sxx == 0:
        raise ValueError("all x values are identical")
    slope = sum((x - x_bar) * (y - y_bar) for x, y in zip(xs, ys)) / sxx
    intercept = y_bar - slope * x_bar
    fitted = [intercept + slope * x for x in xs]
    sst = sum((y - y_bar) ** 2 for y in ys)
    sse = sum((y - yhat) ** 2 for y, yhat in zip(ys, fitted))
    r2 = 1.0 - sse / sst if sst > 0 else 1.0
    return slope, intercept, r2


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path")
    parser.add_argument("--min-fraction", type=float, default=0.02)
    parser.add_argument("--max-fraction", type=float, default=0.95)
    args = parser.parse_args()

    grouped: dict[int, list[float]] = defaultdict(list)
    with open(args.csv_path, newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            grouped[int(row["threshold"])].append(float(row["large_fraction"]))

    points: list[tuple[int, float]] = []
    for threshold, values in sorted(grouped.items()):
        mean_fraction = statistics.fmean(values)
        if args.min_fraction <= mean_fraction <= args.max_fraction:
            points.append((threshold, mean_fraction))

    if len(points) < 3:
        raise SystemExit("need at least three thresholds in the scaling range")

    xs = [math.log(threshold) for threshold, _ in points]
    ys = [math.log(fraction) for _, fraction in points]
    slope, intercept, r2 = ols(xs, ys)
    kappa = -slope

    print("Direct reachable-set tail model")
    print("p_L(B) = P(R >= B) ~= C * B^(-kappa)")
    print(f"kappa={kappa:.6g}")
    print(f"C={math.exp(intercept):.6g}")
    print(f"R^2={r2:.6g}")
    print("points:")
    for threshold, fraction in points:
        print(f"  B={threshold:g}, mean p_L={fraction:.6g}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
