#!/usr/bin/env python3
"""Aggregate experiment CSVs and create publication-style plots."""

from __future__ import annotations

import argparse
import pathlib
import textwrap

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


NUMERIC_COLUMNS = [
    "repeat",
    "n",
    "workers",
    "split_depth",
    "abp_capacity",
    "chase_log_capacity",
    "steal_attempt_budget",
    "steal_attempts",
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
    "steal_aborts",
    "idle_seconds",
    "load_min",
    "load_max",
    "load_mean",
    "load_stddev",
    "abp_overflows",
    "chase_lev_resizes",
    "inline_overflow_tasks",
]


SCHEDULER_LABELS = {
    "global": "Global Queue",
    "abp": "ABP",
    "chaselev": "Chase-Lev",
}


COLORS = {
    "global": "#4c78a8",
    "abp": "#f58518",
    "chaselev": "#54a24b",
}


def load_results(path: pathlib.Path) -> pd.DataFrame:
    df = pd.read_csv(path)
    for column in NUMERIC_COLUMNS:
        if column in df.columns:
            df[column] = pd.to_numeric(df[column], errors="coerce")
    df["scheduler_label"] = df["scheduler"].map(SCHEDULER_LABELS).fillna(df["scheduler"])
    df["load_range"] = df["load_max"] - df["load_min"]
    df["steal_success_rate"] = np.where(
        df["steal_attempts"] > 0,
        df["successful_steals"] / df["steal_attempts"],
        np.nan,
    )
    return df


def summarize(df: pd.DataFrame) -> pd.DataFrame:
    group_cols = [
        "scenario",
        "scheduler",
        "n",
        "workers",
        "split_depth",
        "abp_capacity",
        "chase_log_capacity",
        "steal_attempt_budget",
    ]
    value_cols = [
        "elapsed_seconds",
        "speedup",
        "tasks_per_second",
        "successful_steals",
        "failed_steal_attempts",
        "steal_aborts",
        "idle_seconds",
        "load_stddev",
        "load_range",
        "abp_overflows",
        "chase_lev_resizes",
        "inline_overflow_tasks",
    ]
    summary = (
        df.groupby(group_cols, dropna=False)[value_cols]
        .agg(["median", "mean", "std", "min", "max"])
        .reset_index()
    )
    summary.columns = [
        "_".join(part for part in column if part).rstrip("_")
        if isinstance(column, tuple)
        else column
        for column in summary.columns
    ]
    return summary


def ensure_dirs(out_dir: pathlib.Path) -> pathlib.Path:
    figures = out_dir / "figures"
    figures.mkdir(parents=True, exist_ok=True)
    return figures


def line_plot(
    data: pd.DataFrame,
    x: str,
    y: str,
    hue: str,
    title: str,
    xlabel: str,
    ylabel: str,
    path: pathlib.Path,
    log_x: bool = False,
    log_y: bool = False,
) -> None:
    fig, ax = plt.subplots(figsize=(8.5, 5.2))
    for key, part in data.groupby(hue):
        scheduler = part["scheduler"].iloc[0] if "scheduler" in part.columns else str(key)
        label = SCHEDULER_LABELS.get(str(key), SCHEDULER_LABELS.get(scheduler, str(key)))
        color = COLORS.get(str(key), COLORS.get(scheduler))
        part = part.sort_values(x)
        ax.plot(part[x], part[y], marker="o", linewidth=2.1, label=label, color=color)
    ax.set_title(title)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    if log_x:
        ax.set_xscale("log", base=2)
    if log_y:
        ax.set_yscale("log")
    ax.grid(True, which="major", alpha=0.28)
    ax.grid(True, which="minor", alpha=0.12)
    ax.legend(frameon=False)
    fig.tight_layout()
    fig.savefig(path, dpi=180)
    plt.close(fig)


def plot_scaling(df: pd.DataFrame, figures: pathlib.Path) -> None:
    data = (
        df[df["scenario"] == "scaling"]
        .groupby(["scheduler", "n", "workers"], as_index=False)
        .agg(
            elapsed_seconds=("elapsed_seconds", "median"),
            speedup=("speedup", "median"),
            tasks_per_second=("tasks_per_second", "median"),
            load_stddev=("load_stddev", "median"),
        )
    )
    if data.empty:
        return
    for n, part in data.groupby("n"):
        line_plot(
            part,
            "workers",
            "speedup",
            "scheduler",
            f"Scaling Speedup, N={n}",
            "Workers",
            "Speedup over sequential",
            figures / f"scaling_speedup_n{int(n)}.png",
            log_x=True,
        )
        line_plot(
            part,
            "workers",
            "elapsed_seconds",
            "scheduler",
            f"Runtime vs Workers, N={n}",
            "Workers",
            "Median runtime (s)",
            figures / f"scaling_runtime_n{int(n)}.png",
            log_x=True,
            log_y=True,
        )
        line_plot(
            part,
            "workers",
            "tasks_per_second",
            "scheduler",
            f"Task Throughput, N={n}",
            "Workers",
            "Tasks completed per second",
            figures / f"scaling_throughput_n{int(n)}.png",
            log_x=True,
        )


def plot_granularity(df: pd.DataFrame, figures: pathlib.Path) -> None:
    data = (
        df[df["scenario"] == "granularity"]
        .groupby(["scheduler", "split_depth"], as_index=False)
        .agg(
            elapsed_seconds=("elapsed_seconds", "median"),
            speedup=("speedup", "median"),
            tasks_created=("tasks_created", "median"),
            successful_steals=("successful_steals", "median"),
            failed_steal_attempts=("failed_steal_attempts", "median"),
            load_stddev=("load_stddev", "median"),
        )
    )
    if data.empty:
        return
    line_plot(
        data,
        "split_depth",
        "elapsed_seconds",
        "scheduler",
        "Granularity: Runtime vs Split Depth",
        "Split depth",
        "Median runtime (s)",
        figures / "granularity_runtime.png",
        log_y=True,
    )
    line_plot(
        data,
        "split_depth",
        "speedup",
        "scheduler",
        "Granularity: Speedup vs Split Depth",
        "Split depth",
        "Speedup over sequential",
        figures / "granularity_speedup.png",
    )
    line_plot(
        data,
        "split_depth",
        "tasks_created",
        "scheduler",
        "Granularity: Task Count Explosion",
        "Split depth",
        "Tasks created",
        figures / "granularity_tasks.png",
        log_y=True,
    )
    line_plot(
        data,
        "split_depth",
        "load_stddev",
        "scheduler",
        "Granularity: Worker Load Imbalance",
        "Split depth",
        "Std. dev. of tasks per worker",
        figures / "granularity_load_imbalance.png",
    )


def plot_capacity_pressure(df: pd.DataFrame, figures: pathlib.Path) -> None:
    abp = (
        df[df["scenario"] == "abp_capacity"]
        .groupby(["abp_capacity"], as_index=False)
        .agg(
            elapsed_seconds=("elapsed_seconds", "median"),
            abp_overflows=("abp_overflows", "median"),
            inline_overflow_tasks=("inline_overflow_tasks", "median"),
            speedup=("speedup", "median"),
        )
        .sort_values("abp_capacity")
    )
    if not abp.empty:
        fig, ax1 = plt.subplots(figsize=(8.5, 5.2))
        ax1.plot(abp["abp_capacity"], abp["elapsed_seconds"], color=COLORS["abp"], marker="o", linewidth=2.1)
        ax1.set_xscale("log", base=2)
        ax1.set_xlabel("ABP per-worker capacity")
        ax1.set_ylabel("Median runtime (s)", color=COLORS["abp"])
        ax1.tick_params(axis="y", labelcolor=COLORS["abp"])
        ax1.grid(True, which="major", alpha=0.28)
        ax2 = ax1.twinx()
        ax2.plot(abp["abp_capacity"], abp["abp_overflows"], color="#b279a2", marker="s", linewidth=2.1)
        ax2.set_ylabel("Median overflow count", color="#b279a2")
        ax2.tick_params(axis="y", labelcolor="#b279a2")
        ax1.set_title("ABP Capacity Pressure")
        fig.tight_layout()
        fig.savefig(figures / "abp_capacity_pressure.png", dpi=180)
        plt.close(fig)

    chase = (
        df[df["scenario"] == "chaselev_resize"]
        .groupby(["chase_log_capacity"], as_index=False)
        .agg(
            elapsed_seconds=("elapsed_seconds", "median"),
            chase_lev_resizes=("chase_lev_resizes", "median"),
            speedup=("speedup", "median"),
        )
        .sort_values("chase_log_capacity")
    )
    if not chase.empty:
        fig, ax1 = plt.subplots(figsize=(8.5, 5.2))
        ax1.plot(chase["chase_log_capacity"], chase["elapsed_seconds"], color=COLORS["chaselev"], marker="o", linewidth=2.1)
        ax1.set_xlabel("Initial log2 capacity")
        ax1.set_ylabel("Median runtime (s)", color=COLORS["chaselev"])
        ax1.tick_params(axis="y", labelcolor=COLORS["chaselev"])
        ax1.grid(True, which="major", alpha=0.28)
        ax2 = ax1.twinx()
        ax2.plot(chase["chase_log_capacity"], chase["chase_lev_resizes"], color="#e45756", marker="s", linewidth=2.1)
        ax2.set_ylabel("Median resize count", color="#e45756")
        ax2.tick_params(axis="y", labelcolor="#e45756")
        ax1.set_title("Chase-Lev Resize Pressure")
        fig.tight_layout()
        fig.savefig(figures / "chaselev_resize_pressure.png", dpi=180)
        plt.close(fig)


def plot_steal_attempts(df: pd.DataFrame, figures: pathlib.Path) -> None:
    data = (
        df[df["scenario"] == "steal_attempts"]
        .groupby(["scheduler", "steal_attempt_budget"], as_index=False)
        .agg(
            elapsed_seconds=("elapsed_seconds", "median"),
            successful_steals=("successful_steals", "median"),
            failed_steal_attempts=("failed_steal_attempts", "median"),
            speedup=("speedup", "median"),
        )
    )
    if data.empty:
        return
    line_plot(
        data,
        "steal_attempt_budget",
        "elapsed_seconds",
        "scheduler",
        "Victim Probe Budget: Runtime",
        "Steal attempts per empty poll",
        "Median runtime (s)",
        figures / "steal_attempts_runtime.png",
        log_x=True,
    )
    line_plot(
        data,
        "steal_attempt_budget",
        "failed_steal_attempts",
        "scheduler",
        "Victim Probe Budget: Failed Steals",
        "Steal attempts per empty poll",
        "Median failed steal attempts",
        figures / "steal_attempts_failed.png",
        log_x=True,
        log_y=True,
    )


def write_report(df: pd.DataFrame, summary: pd.DataFrame, out_dir: pathlib.Path) -> None:
    lines: list[str] = ["# Experiment Analysis", ""]
    lines.append(f"Raw rows: {len(df)}")
    lines.append("")

    incorrect = df[df["correct"] != 1]
    if incorrect.empty:
        lines.append("All benchmark runs matched known N-Queens solution counts where known.")
    else:
        lines.append(f"WARNING: {len(incorrect)} runs did not match known solution counts.")
    lines.append("")

    scaling = df[df["scenario"] == "scaling"]
    if not scaling.empty:
        lines.append("## Scaling")
        best = (
            scaling.groupby(["n", "scheduler"], as_index=False)
            .agg(best_speedup=("speedup", "max"), min_runtime=("elapsed_seconds", "min"))
            .sort_values(["n", "best_speedup"], ascending=[True, False])
        )
        for n, part in best.groupby("n"):
            leader = part.iloc[0]
            lines.append(
                f"- N={int(n)}: best speedup came from {SCHEDULER_LABELS.get(leader.scheduler, leader.scheduler)} "
                f"at {leader.best_speedup:.2f}x; fastest observed runtime was {leader.min_runtime:.6f}s."
            )
        lines.append("")

    granularity = df[df["scenario"] == "granularity"]
    if not granularity.empty:
        lines.append("## Granularity")
        med = (
            granularity.groupby(["scheduler", "split_depth"], as_index=False)
            .agg(elapsed_seconds=("elapsed_seconds", "median"), tasks_created=("tasks_created", "median"))
        )
        for scheduler, part in med.groupby("scheduler"):
            row = part.sort_values("elapsed_seconds").iloc[0]
            lines.append(
                f"- {SCHEDULER_LABELS.get(scheduler, scheduler)} was fastest at split depth "
                f"{int(row.split_depth)} with median runtime {row.elapsed_seconds:.6f}s "
                f"and {int(row.tasks_created)} created tasks."
            )
        lines.append("")

    abp = df[df["scenario"] == "abp_capacity"]
    if not abp.empty:
        lines.append("## ABP Capacity")
        med = (
            abp.groupby("abp_capacity", as_index=False)
            .agg(elapsed_seconds=("elapsed_seconds", "median"), abp_overflows=("abp_overflows", "median"))
            .sort_values("abp_capacity")
        )
        first_no_overflow = med[med["abp_overflows"] == 0]
        if not first_no_overflow.empty:
            capacity = int(first_no_overflow.iloc[0].abp_capacity)
            lines.append(f"- ABP first reached zero median overflows at capacity {capacity}.")
        else:
            lines.append("- ABP overflowed for every tested capacity.")
        fastest = med.sort_values("elapsed_seconds").iloc[0]
        lines.append(
            f"- Fastest ABP capacity setting was {int(fastest.abp_capacity)} "
            f"with median runtime {fastest.elapsed_seconds:.6f}s."
        )
        lines.append("")

    chase = df[df["scenario"] == "chaselev_resize"]
    if not chase.empty:
        lines.append("## Chase-Lev Resize")
        med = (
            chase.groupby("chase_log_capacity", as_index=False)
            .agg(elapsed_seconds=("elapsed_seconds", "median"), chase_lev_resizes=("chase_lev_resizes", "median"))
            .sort_values("chase_log_capacity")
        )
        fastest = med.sort_values("elapsed_seconds").iloc[0]
        lines.append(
            f"- Fastest initial log2 capacity was {int(fastest.chase_log_capacity)} "
            f"with median runtime {fastest.elapsed_seconds:.6f}s and "
            f"{fastest.chase_lev_resizes:.0f} median resizes."
        )
        lines.append("")

    lines.append("## Generated Files")
    lines.append("- `summary.csv`: grouped median/mean/std/min/max metrics.")
    lines.append("- `figures/`: runtime, speedup, throughput, granularity, overflow, and resize plots.")
    lines.append("")
    out_dir.joinpath("analysis_report.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("raw_csv", type=pathlib.Path)
    parser.add_argument("--out-dir", type=pathlib.Path)
    args = parser.parse_args()

    out_dir = args.out_dir or args.raw_csv.parent
    out_dir.mkdir(parents=True, exist_ok=True)
    figures = ensure_dirs(out_dir)

    plt.style.use("seaborn-v0_8-whitegrid")
    df = load_results(args.raw_csv)
    summary = summarize(df)
    summary.to_csv(out_dir / "summary.csv", index=False)

    plot_scaling(df, figures)
    plot_granularity(df, figures)
    plot_capacity_pressure(df, figures)
    plot_steal_attempts(df, figures)
    write_report(df, summary, out_dir)
    print(textwrap.dedent(
        f"""
        wrote:
          {out_dir / 'summary.csv'}
          {out_dir / 'analysis_report.md'}
          {figures}
        """
    ).strip())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
