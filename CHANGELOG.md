# Changelog

## 0.3.0

- Changed temporal-path semantics from non-strict to strict timestamps:
  `t_1 < t_2 < ... < t_k`.
- Replaced equal-time fixed-point relaxation with simultaneous batch commits.
- Prevented promotions created at time `t` from propagating through another edge
  at the same time.
- Updated exact reference scan, deterministic tests, randomized differential
  tests, documentation and example data for strict semantics.

## 0.2.0

- Rebuilt the index around an explicit earliest-arrival API.
- Added immediate promotion after an edge from a previously large predecessor.
- Added an independent exact temporal scan.
- Added deterministic tests, randomized all-pairs differential tests and sanitizer CI.
- Added CSV CLI, reproducible benchmark runs and per-query exactness checks.
- Replaced the primary synthetic formula with the direct reachable-set tail law
  `p_L(B) = P(R >= B)`.
- Reclassified the degree-mediated exponent `(gamma - 1) / delta` as a secondary,
  empirically testable hypothesis.
- Replaced the memory equality with the exact truncated expectation and retained
  `n B (1 - p_L(B))` only as an upper bound.
