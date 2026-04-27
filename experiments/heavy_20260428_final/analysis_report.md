# Experiment Analysis

Raw rows: 330

All benchmark runs matched known N-Queens solution counts where known.

## Scaling
- N=12: best speedup came from ABP at 1.65x; fastest observed runtime was 0.004199s.
- N=13: best speedup came from ABP at 1.62x; fastest observed runtime was 0.022622s.
- N=14: best speedup came from ABP at 4.40x; fastest observed runtime was 0.048234s.
- N=15: best speedup came from ABP at 3.05x; fastest observed runtime was 0.433568s.

## Granularity
- ABP was fastest at split depth 5 with median runtime 0.008885s and 38680 created tasks.
- Chase-Lev was fastest at split depth 5 with median runtime 0.009727s and 38680 created tasks.
- Global Queue was fastest at split depth 2 with median runtime 0.009694s and 146 created tasks.

## ABP Capacity
- ABP first reached zero median overflows at capacity 64.
- Fastest ABP capacity setting was 8 with median runtime 0.100735s.

## Chase-Lev Resize
- Fastest initial log2 capacity was 1 with median runtime 0.153091s and 40 median resizes.

## Generated Files
- `summary.csv`: grouped median/mean/std/min/max metrics.
- `figures/`: runtime, speedup, throughput, granularity, overflow, and resize plots.
