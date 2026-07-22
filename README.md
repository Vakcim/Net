# Hybrid bi-level index for temporal reachability

A clean reference implementation and reproducible test/benchmark harness for an
exact hybrid index over a stream of directed temporal edges.

## Semantics

A temporal path uses strictly increasing timestamps:

```text
t1 < t2 < ... < tk
```

Edges must be supplied in nondecreasing time order. Equal-time edges are
processed as one simultaneous batch: reachability created at time `t` cannot be
used by another edge at time `t`. Thus results do not depend on order inside the
batch.
The query returns the earliest arrival timestamp. For `source == target`, it
returns `0` for the empty path.

## Algorithm

- **Small vertices** store exact predecessor labels `source -> earliest arrival`.
- **Large vertices** retain only incoming temporal edges.
- A vertex is promoted when its label count reaches threshold `B`.
- An edge from a predecessor that was already large before the current
  timestamp immediately promotes a small target. A vertex promoted inside the
  current equal-time batch does not propagate its new predecessors at that time.
- Queries to small targets are hash lookups. Queries to large targets perform an
  exact backward traversal and use small labels as shortcuts.

See [docs/ALGORITHM.md](docs/ALGORITHM.md) for the exactness invariant and the
strict equal-time batch rule.

## Build and test

Requirements: CMake 3.16+, a C++20 compiler, Python 3.9+ for benchmark scripts.
No third-party C++ libraries are required.

```bash
make test
make sanitize
```

The test suite includes deterministic examples and 150 randomized graphs. Every
answer is compared with an independent exact temporal scan for multiple
thresholds and all source-target pairs.

## Run an example

```bash
make example
```

Or directly:

```bash
./build/temporal_index_cli \
  --vertices 4 \
  --threshold 2 \
  --input data/example.csv \
  --source 0 \
  --target 3
```

CSV format:

```text
source,target,time
0,1,4
1,2,5
2,3,8
```

## Reproduce the synthetic benchmark

```bash
make benchmark
```

This writes `results/benchmark.csv` and fits the direct promotion law

```text
p_L(B) = P(R >= B) ~= C B^(-kappa),
```

where `R` is the temporal predecessor-set size. See
[docs/SYNTHETIC_VALIDATION.md](docs/SYNTHETIC_VALIDATION.md). The older
`degree -> reachable set` formula is retained as a secondary hypothesis and
must be validated rather than assumed. `make benchmark` also writes a full
per-vertex profile and compares the directly fitted reachable-set exponent with
`(gamma_hat - 1) / delta_hat`.

Two synthetic models are available:

- `dag-pareto` (default): acyclic support, useful for avoiding reachability
  saturation during scaling validation;
- `random-pareto`: cyclic stress test that often becomes highly reachable and
  therefore demonstrates finite-size/saturation failure of naive power-law fits.

Custom benchmark:

```bash
./build/temporal_benchmark \
  --vertices 2000 \
  --events 50000 \
  --queries 10000 \
  --verify-queries 200 \
  --time-buckets 5000 \
  --gamma 2.5 \
  --model dag-pareto \
  --seed 42 \
  --thresholds 4,8,16,32,64,128,256,512 > results/run.csv
```

## Repository layout

```text
include/temporal_index/  public API
src/                     implementation
apps/                    CSV command-line tool
tests/                   exactness and fuzz tests
benchmarks/              deterministic synthetic benchmark
scripts/                 repeated runs and power-law fit
docs/                    algorithm and analytical model notes
```

## Research checklist

Before using results in a paper:

1. run tests and sanitizers on the exact commit being evaluated;
2. record compiler, CPU, OS, commit hash, seed and all CLI parameters;
3. report p50/p95/p99 query latency, build time, label entries and large fraction;
4. require `verification_mismatches = 0` for every benchmark row;
5. fit power laws only on a declared non-saturated scaling range;
6. publish raw CSV files and scripts together with figures.
