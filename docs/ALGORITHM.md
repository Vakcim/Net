# Exactness invariant for strict temporal paths

The implementation uses strict temporal paths:

\[
t_1 < t_2 < \cdots < t_k.
\]

Edges are supplied in nondecreasing timestamp order. Edges with one timestamp
are processed as a simultaneous batch. Reachability created at time `t` is
committed only after every edge at time `t` has been inspected, so it cannot be
used by another edge with the same timestamp.

For every **small** vertex `v`, `labels[v][s]` stores the exact earliest arrival
time of source `s` at `v`. A vertex is promoted when its label count reaches
`B`. Labels are discarded for large vertices, while all incoming temporal edges
are retained.

## Closure-preserving promotion

Suppose `u` is already large before processing timestamp `t`, and an edge
`u -> v @ t` appears. When `u` was promoted at an earlier timestamp, it had at
least `B` temporal predecessors. Their arrival times are strictly smaller than
`t`, so all of them can traverse `u -> v @ t`. Therefore a small `v` is promoted
immediately.

A vertex that becomes large *during* timestamp `t` does not force another target
to become large through an edge with the same timestamp. Its newly discovered
predecessors arrive at time `t` and cannot traverse another edge at time `t`
under strict semantics. This is why equal-time edges are committed as one batch.

## Update rule

For a batch with timestamp `t`, all updates use the state after timestamps
strictly smaller than `t`:

1. retain every incoming edge for later exact backward queries;
2. for `u -> v @ t` with small `u` and small `v`, add `u` and every label of
   `u` to the pending additions of `v`, all with arrival time `t`;
3. for `u -> v @ t` with `u` already large before the batch, mark `v` for
   promotion;
4. after all edges in the batch are inspected, commit additions and promotions.

## Queries

Small targets use one hash lookup. Large targets use an exact backward traversal.
When the traversal follows an incoming edge with time `t`, the path reaching its
source must end at a time strictly smaller than `t`. Small label tables act as
exact shortcuts. Incoming edges of the final target are scanned by ascending
time, so the first feasible one gives the earliest arrival.
