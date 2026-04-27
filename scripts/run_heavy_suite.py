#!/usr/bin/env python3
"""Run a reproducible experiment suite for the work-stealing schedulers."""

from __future__ import annotations

import argparse
import csv
import json
import pathlib
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime
from statistics import median
from typing import Iterable


ROOT = pathlib.Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class Case:
    scenario: str
    scheduler: str
    n: int
    workers: int
    split_depth: int
    repeat: int
    abp_capacity: int = 4096
    chase_log_capacity: int = 4
    steal_attempt_budget: int = 0


def default_executable() -> pathlib.Path:
    exe = ROOT / "build" / "ws_bench.exe"
    if exe.exists():
        return exe
    return ROOT / "build" / "ws_bench"


def parse_int_list(values: Iterable[str]) -> list[int]:
    result: list[int] = []
    for value in values:
        for part in value.split(","):
            part = part.strip()
            if part:
                result.append(int(part))
    return result


def run_csv_command(command: list[str]) -> dict[str, str]:
    completed = subprocess.run(
        command,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    lines = [line for line in completed.stdout.splitlines() if line.strip()]
    rows = list(csv.DictReader(lines))
    if len(rows) != 1:
        raise RuntimeError(f"expected one CSV row from {' '.join(command)}, got {len(rows)}")
    return rows[0]


def measure_sequential(exe: pathlib.Path, ns: Iterable[int], repeats: int) -> dict[int, dict[str, float]]:
    baselines: dict[int, dict[str, float]] = {}
    for n in sorted(set(ns)):
        samples: list[float] = []
        for repeat in range(repeats):
            row = run_csv_command(
                [
                    str(exe),
                    "--scheduler",
                    "global",
                    "--n",
                    str(n),
                    "--workers",
                    "1",
                    "--split-depth",
                    "0",
                    "--seed",
                    str(1000 + repeat),
                    "--csv",
                ]
            )
            samples.append(float(row["sequential_seconds"]))
            print(f"baseline n={n} repeat={repeat} sequential={samples[-1]:.6f}s", file=sys.stderr)
        baselines[n] = {
            "median_seconds": median(samples),
            "mean_seconds": sum(samples) / len(samples),
            "samples": samples,
        }
    return baselines


def build_cases(
    repeats: int,
    max_workers: int,
    quick: bool,
    abp_capacity: int,
    chase_log_capacity: int,
) -> list[Case]:
    schedulers = ["global", "abp", "chaselev"]
    cases: list[Case] = []

    scaling_ns = [12, 13] if quick else [12, 13, 14, 15]
    scaling_workers = [1, 2, 4, 8, 16]
    scaling_workers = [w for w in scaling_workers if w <= max_workers]
    for n in scaling_ns:
        split_depth = 5 if n <= 12 else (6 if n <= 14 else 7)
        for workers in scaling_workers:
            for scheduler in schedulers:
                for repeat in range(repeats):
                    cases.append(
                        Case(
                            scenario="scaling",
                            scheduler=scheduler,
                            n=n,
                            workers=workers,
                            split_depth=split_depth,
                            repeat=repeat,
                            abp_capacity=abp_capacity,
                            chase_log_capacity=chase_log_capacity,
                        )
                    )

    granularity_depths = [2, 3, 4, 5, 6] if quick else [2, 3, 4, 5, 6, 7, 8]
    granularity_workers = min(max_workers, 8)
    granularity_n = 13
    for split_depth in granularity_depths:
        for scheduler in schedulers:
            for repeat in range(repeats):
                cases.append(
                    Case(
                        scenario="granularity",
                        scheduler=scheduler,
                        n=granularity_n,
                        workers=granularity_workers,
                        split_depth=split_depth,
                        repeat=repeat,
                        abp_capacity=abp_capacity,
                        chase_log_capacity=chase_log_capacity,
                    )
                )

    capacity_values = [8, 16, 32, 64, 128, 256] if quick else [4, 8, 16, 32, 64, 128, 256, 512, 1024, 4096]
    for capacity in capacity_values:
        for repeat in range(repeats):
            cases.append(
                Case(
                    scenario="abp_capacity",
                    scheduler="abp",
                    n=13,
                    workers=min(max_workers, 8),
                    split_depth=7,
                    repeat=repeat,
                    abp_capacity=capacity,
                    chase_log_capacity=chase_log_capacity,
                )
            )

    log_capacity_values = [1, 2, 3, 4] if quick else [1, 2, 3, 4, 5, 6, 7]
    for log_capacity in log_capacity_values:
        for repeat in range(repeats):
            cases.append(
                Case(
                    scenario="chaselev_resize",
                    scheduler="chaselev",
                    n=13,
                    workers=min(max_workers, 8),
                    split_depth=7,
                    repeat=repeat,
                    abp_capacity=abp_capacity,
                    chase_log_capacity=log_capacity,
                )
            )

    steal_attempt_values = [1, 2, 4, 8, 16] if quick else [1, 2, 4, 8, 16, 32]
    for attempts in steal_attempt_values:
        for scheduler in ["abp", "chaselev"]:
            for repeat in range(repeats):
                cases.append(
                    Case(
                        scenario="steal_attempts",
                        scheduler=scheduler,
                        n=13,
                        workers=min(max_workers, 8),
                        split_depth=6,
                        repeat=repeat,
                        abp_capacity=abp_capacity,
                        chase_log_capacity=chase_log_capacity,
                        steal_attempt_budget=attempts,
                    )
                )

    return cases


def run_case(exe: pathlib.Path, case: Case, sequential_seconds: float) -> dict[str, str]:
    command = [
        str(exe),
        "--scheduler",
        case.scheduler,
        "--n",
        str(case.n),
        "--workers",
        str(case.workers),
        "--split-depth",
        str(case.split_depth),
        "--abp-capacity",
        str(case.abp_capacity),
        "--chase-log-capacity",
        str(case.chase_log_capacity),
        "--steal-attempts",
        str(case.steal_attempt_budget),
        "--seed",
        str(1 + case.repeat),
        "--csv",
        "--skip-sequential",
    ]
    row = run_csv_command(command)
    row.update({key: str(value) for key, value in asdict(case).items()})
    row["sequential_seconds"] = f"{sequential_seconds:.9f}"
    elapsed = float(row["elapsed_seconds"])
    row["speedup"] = f"{(sequential_seconds / elapsed) if elapsed > 0 else 0.0:.9f}"
    row["command"] = " ".join(command)
    return row


def write_csv(path: pathlib.Path, rows: list[dict[str, str]], fieldnames: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=pathlib.Path, default=default_executable())
    parser.add_argument("--out-dir", type=pathlib.Path)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--baseline-repeats", type=int, default=3)
    parser.add_argument("--max-workers", type=int, default=16)
    parser.add_argument("--abp-capacity", type=int, default=4096)
    parser.add_argument("--chase-log-capacity", type=int, default=4)
    parser.add_argument("--quick", action="store_true")
    args = parser.parse_args()

    if not args.exe.exists():
        print(f"benchmark executable not found: {args.exe}", file=sys.stderr)
        return 1

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = args.out_dir or (ROOT / "experiments" / timestamp)
    out_dir.mkdir(parents=True, exist_ok=True)

    cases = build_cases(
        repeats=args.repeats,
        max_workers=args.max_workers,
        quick=args.quick,
        abp_capacity=args.abp_capacity,
        chase_log_capacity=args.chase_log_capacity,
    )
    ns = [case.n for case in cases]

    metadata = {
        "created_at": timestamp,
        "exe": str(args.exe),
        "repeats": args.repeats,
        "baseline_repeats": args.baseline_repeats,
        "max_workers": args.max_workers,
        "abp_capacity": args.abp_capacity,
        "chase_log_capacity": args.chase_log_capacity,
        "quick": args.quick,
        "case_count": len(cases),
    }
    (out_dir / "metadata.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")

    baselines = measure_sequential(args.exe, ns, args.baseline_repeats)
    baseline_rows = [
        {
            "n": str(n),
            "median_seconds": f"{values['median_seconds']:.9f}",
            "mean_seconds": f"{values['mean_seconds']:.9f}",
            "samples": json.dumps(values["samples"]),
        }
        for n, values in sorted(baselines.items())
    ]
    write_csv(out_dir / "sequential_baselines.csv", baseline_rows, ["n", "median_seconds", "mean_seconds", "samples"])

    rows: list[dict[str, str]] = []
    start = time.perf_counter()
    for index, case in enumerate(cases, start=1):
        row = run_case(args.exe, case, baselines[case.n]["median_seconds"])
        rows.append(row)
        elapsed = time.perf_counter() - start
        print(
            f"[{index}/{len(cases)}] {case.scenario} scheduler={case.scheduler} "
            f"n={case.n} workers={case.workers} split={case.split_depth} "
            f"repeat={case.repeat} elapsed={row['elapsed_seconds']}s total={elapsed:.1f}s",
            file=sys.stderr,
        )

    fieldnames = [
        "scenario",
        "repeat",
        "scheduler",
        "n",
        "workers",
        "split_depth",
        "abp_capacity",
        "chase_log_capacity",
        "steal_attempt_budget",
        "solutions",
        "known_solutions",
        "correct",
        "elapsed_seconds",
        "sequential_seconds",
        "speedup",
        "tasks_created",
        "tasks_scheduled",
        "tasks_completed",
        "tasks_per_second",
        "successful_steals",
        "failed_steal_attempts",
        "steal_attempts",
        "steal_aborts",
        "idle_seconds",
        "load_min",
        "load_max",
        "load_mean",
        "load_stddev",
        "abp_overflows",
        "chase_lev_resizes",
        "inline_overflow_tasks",
        "command",
    ]
    write_csv(out_dir / "raw_results.csv", rows, fieldnames)
    print(f"wrote {len(rows)} rows to {out_dir / 'raw_results.csv'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
