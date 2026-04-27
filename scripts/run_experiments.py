#!/usr/bin/env python3
"""Run the scheduler/N-Queens experiment matrix and write one CSV file."""

from __future__ import annotations

import argparse
import csv
import pathlib
import subprocess
import sys
from typing import Iterable


def parse_int_list(values: Iterable[str]) -> list[int]:
    result: list[int] = []
    for value in values:
        for part in value.split(","):
            part = part.strip()
            if part:
                result.append(int(part))
    return result


def default_executable() -> pathlib.Path:
    root = pathlib.Path(__file__).resolve().parents[1]
    exe = root / "build" / "ws_bench.exe"
    if exe.exists():
        return exe
    return root / "build" / "ws_bench"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=pathlib.Path, default=default_executable())
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("results.csv"))
    parser.add_argument("--schedulers", nargs="+", default=["global", "abp", "chaselev"])
    parser.add_argument("--ns", nargs="+", default=["11", "12", "13"])
    parser.add_argument("--workers", nargs="+", default=["1", "2", "4", "8"])
    parser.add_argument("--split-depths", nargs="+", default=["3", "4", "5", "6"])
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--abp-capacity", type=int, default=4096)
    parser.add_argument("--chase-log-capacity", type=int, default=4)
    parser.add_argument("--steal-attempts", type=int, default=0)
    parser.add_argument("--skip-sequential", action="store_true")
    args = parser.parse_args()

    if not args.exe.exists():
        print(f"benchmark executable not found: {args.exe}", file=sys.stderr)
        return 1

    ns = parse_int_list(args.ns)
    workers = parse_int_list(args.workers)
    split_depths = parse_int_list(args.split_depths)

    rows: list[dict[str, str]] = []
    fieldnames: list[str] | None = None
    for n in ns:
        for split_depth in split_depths:
            for worker_count in workers:
                for scheduler in args.schedulers:
                    for repeat in range(args.repeats):
                        command = [
                            str(args.exe),
                            "--scheduler",
                            scheduler,
                            "--n",
                            str(n),
                            "--workers",
                            str(worker_count),
                            "--split-depth",
                            str(split_depth),
                            "--abp-capacity",
                            str(args.abp_capacity),
                            "--chase-log-capacity",
                            str(args.chase_log_capacity),
                            "--steal-attempts",
                            str(args.steal_attempts),
                            "--seed",
                            str(1 + repeat),
                            "--csv",
                        ]
                        if args.skip_sequential:
                            command.append("--skip-sequential")

                        completed = subprocess.run(
                            command,
                            check=True,
                            text=True,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                        )
                        lines = [line for line in completed.stdout.splitlines() if line.strip()]
                        reader = csv.DictReader(lines)
                        for row in reader:
                            row["repeat"] = str(repeat)
                            if fieldnames is None:
                                fieldnames = ["repeat"] + list(reader.fieldnames or [])
                            rows.append(row)
                        print(
                            f"done scheduler={scheduler} n={n} workers={worker_count} "
                            f"split_depth={split_depth} repeat={repeat}",
                            file=sys.stderr,
                        )

    if fieldnames is None:
        print("no experiment rows produced", file=sys.stderr)
        return 1

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    print(f"wrote {len(rows)} rows to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
