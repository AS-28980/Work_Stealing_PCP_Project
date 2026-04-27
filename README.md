# Comparative Work-Stealing Schedulers

This project implements the three schedulers from the presentation and runs them on the same N-Queens workload:

- `global`: one shared FIFO queue protected by a mutex.
- `abp`: bounded per-worker work-stealing deques in the ABP style.
- `chaselev`: dynamically growing circular per-worker work-stealing deques in the Chase-Lev style.

The chapter material matches the presentation: the bounded work-stealing deque corresponds to the ABP family, and the unbounded/dynamic circular deque corresponds to Chase-Lev.

## Build

```powershell
make all
```

If `make` is not available, the benchmark can be built directly:

```powershell
mkdir build
g++ -std=c++17 -O3 -Wall -Wextra -pedantic -pthread -Iinclude src/ws_bench.cpp -o build/ws_bench.exe
```

## Run

```powershell
.\build\ws_bench.exe --scheduler all --n 12 --workers 4 --split-depth 5
```

CSV output:

```powershell
.\build\ws_bench.exe --scheduler chaselev --n 13 --workers 8 --split-depth 6 --csv
```

## Tests

```powershell
make test
```

The tests verify known N-Queens counts, all scheduler backends, ABP overflow fallback, and Chase-Lev resizing.

## Experiment Matrix

```powershell
python .\scripts\run_experiments.py --out results\nqueens.csv --ns 11 12 13 --workers 1 2 4 8 --split-depths 3 4 5 6 --repeats 3
```

Metrics recorded include total time, speedup over the sequential solver, task throughput, successful steals, failed steal attempts, worker idle time, load imbalance, ABP overflow count, and Chase-Lev resize count.
