#!/usr/bin/env python3
"""Generate the README benchmark chart from benchmark CSV files."""

from __future__ import annotations

import argparse
import csv
import html
import math
from pathlib import Path


THREAD_COUNTS = (12, 24)
COLORS = (
    "#4e79a7",
    "#f28e2b",
    "#59a14f",
    "#e15759",
    "#b07aa1",
    "#76b7b2",
    "#edc948",
    "#ff9da7",
)


def load_rows(input_directory: Path, scene: str | None = None) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for run_order, path in enumerate(sorted(input_directory.glob("*.csv"))):
        with path.open(newline="", encoding="utf-8") as source:
            for row in csv.DictReader(source):
                try:
                    if scene is not None and row["scene"] != scene:
                        continue
                    rows.append(
                        {
                            "machine": row["machine"],
                            "commit": row["git_commit"],
                            "scene": row["scene"],
                            "particles": int(row["particles"]),
                            "threads": int(row["threads"]),
                            "average_ms": float(row["average_update_ms"]),
                            "run": path.name,
                            "run_order": run_order,
                        }
                    )
                except (KeyError, TypeError, ValueError):
                    print(f"Skipping malformed benchmark row in {path}")
    return rows


def group_runs(
    rows: list[dict[str, object]],
) -> list[list[dict[str, object]]]:
    grouped: dict[str, list[dict[str, object]]] = {}
    for row in rows:
        grouped.setdefault(str(row["run"]), []).append(row)
    return sorted(
        grouped.values(),
        key=lambda run: int(run[0]["run_order"]),
    )


def is_particle_scaling_run(run: list[dict[str, object]]) -> bool:
    particle_counts = {int(row["particles"]) for row in run}
    thread_counts = {int(row["threads"]) for row in run}
    return len(particle_counts) > 2 and thread_counts.issubset(THREAD_COUNTS)


def is_thread_scaling_run(run: list[dict[str, object]]) -> bool:
    return len({int(row["threads"]) for row in run}) > 2


def rows_for_run_type(
    rows: list[dict[str, object]], run_type: str
) -> list[dict[str, object]]:
    predicate = (
        is_particle_scaling_run
        if run_type == "particle"
        else is_thread_scaling_run
    )
    return [row for run in group_runs(rows) if predicate(run) for row in run]


def latest_run(
    rows: list[dict[str, object]], run_type: str
) -> list[dict[str, object]] | None:
    predicate = (
        is_particle_scaling_run
        if run_type == "particle"
        else is_thread_scaling_run
    )
    candidates = [run for run in group_runs(rows) if predicate(run)]
    return candidates[-1] if candidates else None


def text(x: float, y: float, value: str, **attributes: object) -> str:
    attrs = " ".join(f'{key.replace("_", "-")}="{item}"' for key, item in attributes.items())
    return f'<text x="{x:.1f}" y="{y:.1f}" {attrs}>{html.escape(value)}</text>'


def placeholder(message: str, width: int = 1400, height: int = 620) -> str:
    return "\n".join(
        (
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
            '<rect width="100%" height="100%" fill="#101418"/>',
            text(width / 2, height / 2, message, fill="#d8dee9", **{"text-anchor": "middle", "font-size": 24}),
            "</svg>",
        )
    )


def render_chart(rows: list[dict[str, object]], commit: str, scene: str) -> str:
    selected = [row for row in rows if row["commit"] == commit and row["threads"] in THREAD_COUNTS]
    if not selected:
        return placeholder(f"No {scene} benchmark results for commit {commit}")

    # Later files replace earlier runs for the same machine/scenario point.
    points: dict[tuple[str, int, int], float] = {}
    for row in selected:
        points[(str(row["machine"]), int(row["threads"]), int(row["particles"]))] = float(row["average_ms"])

    machines = sorted({key[0] for key in points})
    particle_counts = sorted({key[2] for key in points})
    if not particle_counts:
        return placeholder("No benchmark points to plot")

    width = 1400
    height = 620
    margin_left = 82
    margin_right = 38
    margin_top = 135
    margin_bottom = 78
    panel_gap = 55
    panel_width = (
        width - margin_left - margin_right - panel_gap * (len(machines) - 1)
    ) / len(machines)
    panel_height = height - margin_top - margin_bottom
    max_ms = max(points.values())
    y_max = max(1.0, max_ms * 1.1)
    particle_min = min(particle_counts)
    particle_max = max(particle_counts)
    particle_range = max(1, particle_max - particle_min)

    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#101418"/>',
        text(width / 2, 34, "Simulation update time", fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 26, "font-family": "sans-serif"}),
        text(width / 2, 61, f"scene: {scene} · commit: {commit}", fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}),
    ]

    legend_width = len(THREAD_COUNTS) * 150
    legend_x = (width - legend_width) / 2
    for thread_index, threads in enumerate(THREAD_COUNTS):
        color = COLORS[thread_index]
        x = legend_x + thread_index * 150
        svg.append(f'<line x1="{x:.1f}" y1="88" x2="{x + 25:.1f}" y2="88" stroke="{color}" stroke-width="3"/>')
        svg.append(text(x + 33, 93, f"{threads} threads", fill="#d8dee9", **{"font-size": 13, "font-family": "sans-serif"}))

    for machine_index, machine in enumerate(machines):
        x0 = margin_left + machine_index * (panel_width + panel_gap)
        bottom = margin_top + panel_height
        svg.append(f'<rect x="{x0:.1f}" y="{margin_top}" width="{panel_width:.1f}" height="{panel_height}" fill="#171d23" stroke="#45515e"/>')
        svg.append(text(x0 + panel_width / 2, margin_top - 13, machine, fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 18, "font-family": "sans-serif"}))

        for tick in range(6):
            value = y_max * tick / 5
            y = bottom - panel_height * tick / 5
            svg.append(f'<line x1="{x0:.1f}" y1="{y:.1f}" x2="{x0 + panel_width:.1f}" y2="{y:.1f}" stroke="#2b3540"/>')
            if machine_index == 0:
                svg.append(text(x0 - 10, y + 5, f"{value:.1f}", fill="#aeb8c4", **{"text-anchor": "end", "font-size": 13, "font-family": "sans-serif"}))

        for particles in particle_counts:
            x = x0 + panel_width * (particles - particle_min) / particle_range
            if particles % (16 * 1024) == 0 or particles in (particle_min, particle_max):
                svg.append(f'<line x1="{x:.1f}" y1="{bottom:.1f}" x2="{x:.1f}" y2="{bottom + 5:.1f}" stroke="#aeb8c4"/>')
                svg.append(text(x, bottom + 24, f"{particles // 1024}K", fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 12, "font-family": "sans-serif"}))

        for thread_index, threads in enumerate(THREAD_COUNTS):
            color = COLORS[thread_index]
            thread_points = [
                (particles, points[(machine, threads, particles)])
                for particles in particle_counts
                if (machine, threads, particles) in points
            ]
            coordinates = [
                (
                    x0 + panel_width * (particles - particle_min) / particle_range,
                    bottom - panel_height * value / y_max,
                )
                for particles, value in thread_points
            ]
            if len(coordinates) > 1:
                joined = " ".join(f"{x:.1f},{y:.1f}" for x, y in coordinates)
                svg.append(f'<polyline points="{joined}" fill="none" stroke="{color}" stroke-width="3"/>')
            for x, y in coordinates:
                svg.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="4" fill="{color}"/>')

    svg.append(text(width / 2, height - 22, "Particles", fill="#d8dee9", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}))
    svg.append(text(20, margin_top + panel_height / 2, "Average update (ms)", fill="#d8dee9", transform=f"rotate(-90 20 {margin_top + panel_height / 2:.1f})", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}))

    svg.append("</svg>")
    return "\n".join(svg)


def available_thread_scaling_particles(
    rows: list[dict[str, object]], commit: str
) -> list[int]:
    threads_by_particle: dict[int, set[int]] = {}
    for row in rows:
        if row["commit"] != commit:
            continue
        particles = int(row["particles"])
        threads_by_particle.setdefault(particles, set()).add(int(row["threads"]))
    return sorted(
        particles
        for particles, thread_counts in threads_by_particle.items()
        if len(thread_counts) > 2
    )


def render_thread_chart(
    rows: list[dict[str, object]],
    commit: str,
    scene: str,
    requested_particle_counts: set[int] | None = None,
) -> str:
    selected = [
        row
        for row in rows
        if row["commit"] == commit
    ]
    if not selected:
        return placeholder(
            f"No thread-scaling results for commit {commit}"
        )

    threads_by_particle: dict[int, set[int]] = {}
    for row in selected:
        particles = int(row["particles"])
        threads_by_particle.setdefault(particles, set()).add(int(row["threads"]))
    particle_counts = sorted(
        particles
        for particles, thread_counts in threads_by_particle.items()
        if len(thread_counts) > 2
        and (
            requested_particle_counts is None
            or particles in requested_particle_counts
        )
    )
    if not particle_counts:
        return placeholder("No complete thread-scaling particle sets to plot")

    points: dict[tuple[str, int, int], float] = {}
    for row in selected:
        particles = int(row["particles"])
        if particles not in particle_counts:
            continue
        points[(
            str(row["machine"]),
            particles,
            int(row["threads"]),
        )] = float(row["average_ms"])

    machines = sorted({key[0] for key in points})
    thread_counts = sorted({key[2] for key in points})
    if len(thread_counts) < 2:
        return placeholder("Not enough thread-scaling points to plot")

    width = 1400
    height = 620
    margin_left = 82
    margin_right = 38
    margin_top = 135
    margin_bottom = 78
    panel_gap = 55
    panel_width = (
        width - margin_left - margin_right - panel_gap * (len(machines) - 1)
    ) / len(machines)
    plot_height = height - margin_top - margin_bottom
    bottom = margin_top + plot_height
    max_ms = max(points.values())
    y_max = max(1.0, max_ms * 1.1)
    thread_min = min(thread_counts)
    thread_max = max(thread_counts)
    thread_range = max(1, thread_max - thread_min)
    tick_step = max(1, math.ceil(thread_max / 12))

    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#101418"/>',
        text(width / 2, 34, "Thread scaling by particle count", fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 26, "font-family": "sans-serif"}),
        text(width / 2, 61, f"scene: {scene} · commit: {commit}", fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}),
    ]

    legend_width = len(particle_counts) * 115
    legend_x = (width - legend_width) / 2
    for particle_index, particles in enumerate(particle_counts):
        color = COLORS[particle_index % len(COLORS)]
        x = legend_x + particle_index * 115
        svg.append(f'<line x1="{x:.1f}" y1="88" x2="{x + 25:.1f}" y2="88" stroke="{color}" stroke-width="3"/>')
        svg.append(text(x + 33, 93, f"{particles // 1024}K", fill="#d8dee9", **{"font-size": 13, "font-family": "sans-serif"}))

    for machine_index, machine in enumerate(machines):
        x0 = margin_left + machine_index * (panel_width + panel_gap)
        svg.append(f'<rect x="{x0:.1f}" y="{margin_top}" width="{panel_width:.1f}" height="{plot_height}" fill="#171d23" stroke="#45515e"/>')
        svg.append(text(x0 + panel_width / 2, margin_top - 13, machine, fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 18, "font-family": "sans-serif"}))

        for tick in range(6):
            value = y_max * tick / 5
            y = bottom - plot_height * tick / 5
            svg.append(f'<line x1="{x0:.1f}" y1="{y:.1f}" x2="{x0 + panel_width:.1f}" y2="{y:.1f}" stroke="#2b3540"/>')
            if machine_index == 0:
                svg.append(text(x0 - 10, y + 5, f"{value:.1f}", fill="#aeb8c4", **{"text-anchor": "end", "font-size": 13, "font-family": "sans-serif"}))

        for threads in thread_counts:
            if threads != thread_min and threads != thread_max and threads % tick_step:
                continue
            x = x0 + panel_width * (threads - thread_min) / thread_range
            svg.append(f'<line x1="{x:.1f}" y1="{bottom:.1f}" x2="{x:.1f}" y2="{bottom + 5:.1f}" stroke="#aeb8c4"/>')
            svg.append(text(x, bottom + 24, str(threads), fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 12, "font-family": "sans-serif"}))

        for particle_index, particles in enumerate(particle_counts):
            color = COLORS[particle_index % len(COLORS)]
            machine_points = [
                (threads, points[(machine, particles, threads)])
                for threads in thread_counts
                if (machine, particles, threads) in points
            ]
            coordinates = [
                (
                    x0 + panel_width * (threads - thread_min) / thread_range,
                    bottom - plot_height * value / y_max,
                )
                for threads, value in machine_points
            ]
            if len(coordinates) > 1:
                joined = " ".join(f"{x:.1f},{y:.1f}" for x, y in coordinates)
                svg.append(f'<polyline points="{joined}" fill="none" stroke="{color}" stroke-width="3"/>')
            for x, y in coordinates:
                svg.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="4" fill="{color}"/>')

    svg.append(text(width / 2, height - 22, "Worker threads", fill="#d8dee9", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}))
    svg.append(text(20, margin_top + plot_height / 2, "Average update (ms)", fill="#d8dee9", transform=f"rotate(-90 20 {margin_top + plot_height / 2:.1f})", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}))

    svg.append("</svg>")
    return "\n".join(svg)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, default=Path("benchmarks/results"))
    parser.add_argument("--output", type=Path, default=Path("results/benchmark_update_time.svg"))
    parser.add_argument(
        "--thread-output",
        type=Path,
        default=Path("results/benchmark_thread_scaling.svg"),
    )
    parser.add_argument("--scene")
    parser.add_argument("--commit")
    parser.add_argument("--particles-k", type=int)
    args = parser.parse_args()

    all_rows = load_rows(args.input)

    latest_particle_run = latest_run(all_rows, "particle")
    particle_scene = args.scene or (
        str(latest_particle_run[-1]["scene"])
        if latest_particle_run
        else "large_cage"
    )
    particle_commit = args.commit or (
        str(latest_particle_run[-1]["commit"])
        if latest_particle_run
        else "unknown"
    )
    particle_rows = [
        row
        for row in rows_for_run_type(all_rows, "particle")
        if row["scene"] == particle_scene
    ]
    chart = render_chart(
        particle_rows, particle_commit, particle_scene
    ) if particle_rows else placeholder(
        "Run the benchmark suite on this computer, then regenerate this chart"
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(chart, encoding="utf-8")
    print(f"Wrote {args.output}")

    latest_thread_run = latest_run(all_rows, "thread")
    thread_scene = args.scene or (
        str(latest_thread_run[-1]["scene"])
        if latest_thread_run
        else "large_cage"
    )
    thread_commit = args.commit or (
        str(latest_thread_run[-1]["commit"])
        if latest_thread_run
        else "unknown"
    )
    thread_rows = [
        row
        for row in rows_for_run_type(all_rows, "thread")
        if row["scene"] == thread_scene
    ]
    requested_thread_particles = (
        {args.particles_k * 1024}
        if args.particles_k is not None
        else None
    )
    available_thread_particles = available_thread_scaling_particles(
        thread_rows, thread_commit
    )
    thread_chart = (
        render_thread_chart(
            thread_rows,
            thread_commit,
            thread_scene,
            requested_thread_particles,
        )
        if thread_rows and available_thread_particles
        else placeholder("Run the thread-scaling benchmark to generate this chart")
    )
    args.thread_output.parent.mkdir(parents=True, exist_ok=True)
    args.thread_output.write_text(thread_chart, encoding="utf-8")
    print(f"Wrote {args.thread_output}")


if __name__ == "__main__":
    main()
