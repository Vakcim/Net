# Migrating the public `Vakcim/Net` repository

This directory is a clean rewrite based on the algorithm described in the paper,
not a line-by-line modification of the legacy prototype.

The current version implements strict temporal paths `t_1 < ... < t_k`; equal-time events are accepted, but cannot be chained inside one path.

## Safe branch workflow

```bash
git clone https://github.com/Vakcim/Net.git
cd Net

git switch -c legacy-snapshot
git push -u origin legacy-snapshot

git switch main
git switch -c reproducible-rewrite
```

Copy the contents of this package into the repository root, keeping `.git/`, and
remove obsolete tracked files only after checking the backup branch. Then run:

```bash
make test
make sanitize
git add -A
git commit -m "Rebuild temporal index with exact tests and reproducible benchmarks"
git push -u origin reproducible-rewrite
```

Open a pull request from `reproducible-rewrite` to `main`. Do not overwrite
`main` until the CI workflow is green and benchmark verification reports zero
mismatches.

## Suggested follow-up commits

1. `Add exact hybrid temporal index and reference scan`
2. `Add deterministic and randomized correctness tests`
3. `Add synthetic benchmark and reachable-set power-law validation`
4. `Add documentation and GitHub Actions CI`

Splitting the rewrite into these commits makes review easier than one monolithic
replacement commit.
