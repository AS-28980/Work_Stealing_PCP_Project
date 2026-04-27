# Work-Stealing Scheduler Experiment Results

Final run directory: `experiments/heavy_20260428_final`

## What Was Run

- 330 scheduler benchmark runs.
- 3 repeats per configuration.
- Sequential baselines measured separately with 3 repeats per `N`.
- Scaling workload: `N = 12, 13, 14, 15`, workers `1, 2, 4, 8, 16`.
- Granularity workload: `N = 13`, workers `8`, split depths `2..8`.
- ABP capacity pressure: `N = 13`, workers `8`, split depth `7`, capacities `4..4096`.
- Chase-Lev resize pressure: `N = 13`, workers `8`, split depth `7`, initial log2 capacities `1..7`.
- Steal probe budget sweep: `N = 13`, workers `8`, split depth `6`, budgets `1, 2, 4, 8, 16, 32`.

All runs matched the known N-Queens solution counts.

## Sequential Baselines

| N | Median sequential time (s) |
|---:|---:|
| 12 | 0.006947 |
| 13 | 0.036639 |
| 14 | 0.212056 |
| 15 | 1.322492 |

## Scaling Summary

Median speedup over the sequential solver:

### N = 12

| Workers | Global | ABP | Chase-Lev |
|---:|---:|---:|---:|
| 1 | 0.56 | 0.53 | 0.56 |
| 2 | 0.54 | 0.83 | 0.69 |
| 4 | 0.34 | 1.11 | 0.89 |
| 8 | 0.32 | 1.53 | 1.03 |
| 16 | 0.40 | 1.59 | 1.30 |

### N = 13

| Workers | Global | ABP | Chase-Lev |
|---:|---:|---:|---:|
| 1 | 0.51 | 0.57 | 0.51 |
| 2 | 0.45 | 0.86 | 0.73 |
| 4 | 0.41 | 1.26 | 0.98 |
| 8 | 0.33 | 1.20 | 1.34 |
| 16 | 0.31 | 1.59 | 1.35 |

### N = 14

| Workers | Global | ABP | Chase-Lev |
|---:|---:|---:|---:|
| 1 | 0.76 | 0.80 | 0.76 |
| 2 | 0.97 | 1.42 | 1.30 |
| 4 | 1.08 | 2.42 | 2.26 |
| 8 | 0.96 | 3.78 | 2.94 |
| 16 | 0.89 | 4.03 | 3.96 |

### N = 15

| Workers | Global | ABP | Chase-Lev |
|---:|---:|---:|---:|
| 1 | 0.71 | 0.75 | 0.70 |
| 2 | 0.84 | 1.31 | 1.18 |
| 4 | 0.88 | 2.22 | 1.85 |
| 8 | 0.72 | 2.60 | 2.34 |
| 16 | 0.57 | 2.80 | 2.07 |

## Main Takeaways

- The global queue bottlenecks quickly. It is simple and sometimes okay at tiny task counts, but it degrades as worker count and task count grow because every enqueue/dequeue contends on the same lock.
- ABP is generally fastest in this implementation for the tested workloads. Its local push/pop path is very cheap, and with enough capacity it avoids the global scheduler bottleneck.
- Chase-Lev tracks ABP closely at medium-to-large workloads, but dynamic-array management and shared-pointer publication add some overhead in this C++ implementation.
- Split depth matters more than scheduler choice once it becomes too large. At split depth `8`, all schedulers are dominated by task-management overhead.
- For `N=13` on 8 workers, ABP and Chase-Lev were fastest around split depth `5`; the global queue was fastest only at very coarse split depth `2`.
- ABP first reached zero median overflow count at capacity `64`. Smaller capacities run many overflow tasks inline, which can look faster because it suppresses scheduling overhead, but that is no longer a pure bounded-deque scheduling regime.
- Chase-Lev resize counts decrease as expected with larger initial arrays, but runtime was fairly flat for the tested capacities. Avoiding every resize did not automatically make the run faster.

## Important Generated Files

- `raw_results.csv`: every individual benchmark run.
- `sequential_baselines.csv`: repeated sequential timings used for speedup.
- `summary.csv`: grouped median/mean/std/min/max aggregates.
- `analysis_report.md`: automatically generated compact report.
- `figures/`: all generated PNG plots.

