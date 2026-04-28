# Work-Stealing Scheduler Experiment Results

Final run directory: `experiments/heavy_20260428_refactor`

## What Was Run

- 330 scheduler benchmark runs.
- 3 repeats per configuration.
- Sequential baselines measured separately with 3 repeats per `N`.
- Scaling workload: `N = 12, 13, 14, 15`, workers `1, 2, 4, 8, 16`.
- Granularity workload: `N = 13`, workers `8`, split depths `2..8`.
- ABP capacity pressure: `N = 13`, workers `8`, split depth `7`, capacities `4..4096`.
- Chase-Lev resize pressure: `N = 13`, workers `8`, split depth `7`, initial log2 capacities `1..7`.
- Steal probe budget sweep: `N = 13`, workers `8`, split depth `6`, budgets `1, 2, 4, 8, 16, 32`.

All runs matched known N-Queens solution counts.

## Sequential Baselines

| N | Median sequential time (s) |
|---:|---:|
| 12 | 0.012465 |
| 13 | 0.035708 |
| 14 | 0.212926 |
| 15 | 1.328736 |

## Scaling Summary

Median speedup over the sequential solver:

### N = 12

| Workers | Global | ABP | Chase-Lev |
|---:|---:|---:|---:|
| 1 | 1.02 | 1.11 | 0.99 |
| 2 | 0.95 | 1.67 | 1.58 |
| 4 | 0.66 | 1.79 | 1.63 |
| 8 | 0.67 | 2.88 | 2.28 |
| 16 | 0.68 | 2.32 | 1.81 |

### N = 13

| Workers | Global | ABP | Chase-Lev |
|---:|---:|---:|---:|
| 1 | 0.49 | 0.53 | 0.49 |
| 2 | 0.45 | 0.85 | 0.71 |
| 4 | 0.41 | 1.21 | 1.01 |
| 8 | 0.33 | 1.37 | 1.29 |
| 16 | 0.30 | 1.56 | 1.32 |

### N = 14

| Workers | Global | ABP | Chase-Lev |
|---:|---:|---:|---:|
| 1 | 0.75 | 0.78 | 0.75 |
| 2 | 0.99 | 1.42 | 1.31 |
| 4 | 1.09 | 2.46 | 2.15 |
| 8 | 0.97 | 3.64 | 3.02 |
| 16 | 0.92 | 4.13 | 3.73 |

### N = 15

| Workers | Global | ABP | Chase-Lev |
|---:|---:|---:|---:|
| 1 | 0.71 | 0.74 | 0.70 |
| 2 | 0.83 | 1.29 | 1.21 |
| 4 | 0.89 | 2.21 | 2.00 |
| 8 | 0.75 | 2.09 | 2.48 |
| 16 | 0.61 | 2.99 | 2.48 |

## Main Takeaways

- The global queue again shows the expected central-lock bottleneck. It is reasonable only when task count is very small, then it gets worse as worker count rises.
- ABP is the strongest scheduler overall in this run. It wins most scaling points and reaches the highest observed speedup: `4.24x` in the generated report for `N=14`.
- Chase-Lev stays close to ABP and even wins the `N=15`, 8-worker median point, but the dynamic circular array path is generally a little more expensive here.
- Split depth is the biggest tuning knob. At `N=13`, 8 workers, split depths `2..5` are useful; depths `6..8` create enough tasks that scheduling overhead dominates.
- Task counts grow quickly with split depth: `146` tasks at depth `2`, `38,680` at depth `5`, and `1,199,082` at depth `8`.
- ABP first reaches zero median overflow count at capacity `64`.
- Chase-Lev resize counts decrease with larger initial arrays, but runtime stays fairly flat; avoiding all resizes is not automatically best.

## Important Generated Files

- `raw_results.csv`: every individual benchmark run.
- `sequential_baselines.csv`: repeated sequential timings used for speedup.
- `summary.csv`: grouped median/mean/std/min/max aggregates.
- `analysis_report.md`: automatically generated compact report.
- `figures/`: all generated PNG plots.

