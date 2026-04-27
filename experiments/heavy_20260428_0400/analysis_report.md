# Experiment Analysis

Raw rows: 285

All benchmark runs matched known N-Queens solution counts where known.

## Scaling
- N=12: best speedup came from ABP at 1.60x; fastest observed runtime was 0.004146s.
- N=13: best speedup came from ABP at 1.43x; fastest observed runtime was 0.024946s.
- N=14: best speedup came from ABP at 4.52x; fastest observed runtime was 0.046776s.

## Granularity
- ABP was fastest at split depth 2 with median runtime 0.005803s and 146 created tasks.
- Chase-Lev was fastest at split depth 2 with median runtime 0.006029s and 146 created tasks.
- Global Queue was fastest at split depth 2 with median runtime 0.005632s and 146 created tasks.

## ABP Capacity
- ABP first reached zero median overflows at capacity 64.
- Fastest ABP capacity setting was 4 with median runtime 0.055156s.

## Chase-Lev Resize
- Fastest initial log2 capacity was 3 with median runtime 0.082801s and 23 median resizes.

## Generated Files
- `summary.csv`: grouped median/mean/std/min/max metrics.
- `figures/`: runtime, speedup, throughput, granularity, overflow, and resize plots.
