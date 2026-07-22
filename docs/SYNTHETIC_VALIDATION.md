# Synthetic power-law validation

Promotion is controlled by the temporal predecessor-set size

\[
R_v = |\{s : s \leadsto v\}|,
\]

not directly by static degree. Therefore the most robust validation starts from
the empirical tail of `R`.

## Direct model

A vertex is large at threshold `B` exactly when `R_v >= B`, hence

\[
p_L(B) = \Pr(R \ge B) = \bar F_R(B).
\]

If the reachable-set survival function has a power-law tail

\[
\Pr(R \ge r) \sim C r^{-\kappa},
\]

then

\[
p_L(B) \sim C B^{-\kappa}.
\]

This is the recommended primary formula. Estimate `kappa` from the slope of the
empirical CCDF in a non-saturated scaling range; do not fit thresholds where
`p_L` is nearly zero or one.

## Degree-mediated model

The paper's formula is a secondary model. If

\[
\Pr(K \ge k) \sim (k/k_{min})^{1-\gamma}
\]

and

\[
R \approx C_R K^\delta,
\]

then

\[
p_L(B) \sim A B^{-(\gamma-1)/\delta},
\qquad \kappa = \frac{\gamma-1}{\delta}.
\]

A degree-power-law generator does **not** automatically imply a power law for
reachable sets. Validate both assumptions separately: estimate `gamma` from the
indegree CCDF, estimate `delta` from `log R` versus `log K`, and compare the
predicted `kappa` with the direct reachable-set estimate.

## Exact label-memory identity

Let `R_v` be the final predecessor count. Since large tables are discarded,

\[
M_{labels}(B) = \sum_v R_v\,\mathbf{1}\{R_v < B\}
              = n\,\mathbb{E}[R\,\mathbf{1}\{R < B\}].
\]

The expression used in the draft,

\[
nB(1-p_L(B)),
\]

is an upper bound rather than an equality:

\[
M_{labels}(B) \le nB(1-p_L(B)).
\]

For a continuous Pareto tail with
`P(R >= r) = (r/r_min)^(-kappa)`, the untruncated partial expectation is

\[
\mathbb{E}[R\mathbf{1}\{R<B\}] =
\begin{cases}
\dfrac{\kappa r_{min}^{\kappa}}{1-\kappa}
\left(B^{1-\kappa}-r_{min}^{1-\kappa}\right), & \kappa \ne 1,\\[6pt]
r_{min}\log(B/r_{min}), & \kappa=1.
\end{cases}
\]

Finite graphs require a truncated-tail correction near `R = n-1`; empirical
sums are preferable for the main experimental comparison.
