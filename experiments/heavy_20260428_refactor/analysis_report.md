# Experiment Analysis

Raw rows: 330

All benchmark runs matched known N-Queens solution counts where known.

## Scaling
- N=12: best speedup came from ABP at 3.07x; fastest observed runtime was 0.004063s.
- N=13: best speedup came from ABP at 1.59x; fastest observed runtime was 0.022497s.
- N=14: best speedup came from ABP at 4.24x; fastest observed runtime was 0.050231s.
- N=15: best speedup came from ABP at 3.20x; fastest observed runtime was 0.415453s.

## Granularity
- ABP was fastest at split depth 2 with median runtime 0.007751s and 146 created tasks.
- Chase-Lev was fastest at split depth 2 with median runtime 0.008051s and 146 created tasks.
- Global Queue was fastest at split depth 2 with median runtime 0.006076s and 146 created tasks.

## ABP Capacity
- ABP first reached zero median overflows at capacity 64.
- Fastest ABP capacity setting was 128 with median runtime 0.073355s.

## Chase-Lev Resize
- Fastest initial log2 capacity was 2 with median runtime 0.084460s and 32 median resizes.

## Generated Files
- `summary.csv`: grouped median/mean/std/min/max metrics.
- `figures/`: runtime, speedup, throughput, granularity, overflow, and resize plots.
