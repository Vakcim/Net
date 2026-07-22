#!/usr/bin/env python3
"""Compare the direct reachable-set exponent with the degree-mediated prediction."""

from __future__ import annotations

import argparse
import csv
import math
import statistics
from collections import Counter


def ols(xs: list[float], ys: list[float]) -> tuple[float, float, float]:
    xb = statistics.fmean(xs)
    yb = statistics.fmean(ys)
    sxx = sum((x - xb) ** 2 for x in xs)
    if sxx == 0:
        raise ValueError("zero x variance")
    slope = sum((x - xb) * (y - yb) for x, y in zip(xs, ys)) / sxx
    intercept = yb - slope * xb
    sst = sum((y - yb) ** 2 for y in ys)
    sse = sum((y - intercept - slope * x) ** 2 for x, y in zip(xs, ys))
    return slope, intercept, 1 - sse / sst if sst else 1.0


def ccdf_fit(values: list[int], minimum: int, max_fraction: float = 0.95) -> tuple[float, float]:
    positive = [v for v in values if v >= minimum]
    counts = Counter(positive)
    n = len(values)
    tail = 0
    points: list[tuple[int, float]] = []
    for value in sorted(counts, reverse=True):
        tail += counts[value]
        fraction = tail / n
        if 0 < fraction <= max_fraction:
            points.append((value, fraction))
    points.reverse()
    if len(points) < 3:
        raise ValueError("not enough CCDF points")
    slope, _, r2 = ols([math.log(x) for x, _ in points], [math.log(y) for _, y in points])
    return slope, r2


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("profile_csv")
    parser.add_argument("--k-min", type=int, default=2)
    parser.add_argument("--r-min", type=int, default=2)
    args = parser.parse_args()

    indegree: list[int] = []
    reachable: list[int] = []
    with open(args.profile_csv, newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            indegree.append(int(row["indegree"]))
            reachable.append(int(row["predecessor_count"]))

    degree_slope, degree_r2 = ccdf_fit(indegree, args.k_min)
    gamma_hat = 1 - degree_slope
    reach_slope, reach_r2 = ccdf_fit(reachable, args.r_min)
    kappa_direct = -reach_slope

    pairs = [(k, r) for k, r in zip(indegree, reachable) if k > 0 and r > 0]
    delta, _, delta_r2 = ols([math.log(k) for k, _ in pairs], [math.log(r) for _, r in pairs])
    kappa_predicted = (gamma_hat - 1) / delta if delta > 0 else float("nan")

    print(f"gamma_hat={gamma_hat:.6g}  degree_ccdf_R2={degree_r2:.6g}")
    print(f"delta_hat={delta:.6g}  logR_logK_R2={delta_r2:.6g}")
    print(f"kappa_direct={kappa_direct:.6g}  reach_ccdf_R2={reach_r2:.6g}")
    print(f"kappa_predicted=(gamma_hat-1)/delta={kappa_predicted:.6g}")
    print(f"absolute_gap={abs(kappa_direct-kappa_predicted):.6g}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
