#!/usr/bin/env python3
"""Generate the README benchmark chart from benchmark CSV files."""

from __future__ import annotations

import argparse
import csv
import html
import math
import re
from pathlib import Path


THREAD_COUNTS = (12, 24)
THREAD_SCALING_REFERENCE_PARTICLES = 32 * 1024
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
                            "cpu": row.get("cpu", "unknown-cpu"),
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


def public_cpu_name(cpu: str) -> str:
    name = cpu.replace("(R)", "").replace("(TM)", "")
    name = re.sub(r"\s+\d+-Core Processor\s*$", "", name)
    name = re.sub(r"\s+Processor\s*$", "", name)
    return " ".join(name.split()) or "Unknown CPU"


def cpu_labels_by_machine(
    rows: list[dict[str, object]], machines: list[str]
) -> dict[str, str]:
    cpu_by_machine = {
        machine: public_cpu_name(str(next(
            row.get("cpu", "unknown-cpu")
            for row in reversed(rows)
            if str(row["machine"]) == machine
        )))
        for machine in machines
    }
    counts: dict[str, int] = {}
    for cpu in cpu_by_machine.values():
        counts[cpu] = counts.get(cpu, 0) + 1
    return {
        machine: (
            f"{cpu} ({machine})" if counts[cpu] > 1 else cpu
        )
        for machine, cpu in cpu_by_machine.items()
    }


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


def latest_commit_per_machine(
    rows: list[dict[str, object]], run_type: str, scene: str
) -> dict[str, str]:
    predicate = (
        is_particle_scaling_run
        if run_type == "particle"
        else is_thread_scaling_run
    )
    commits: dict[str, str] = {}
    for run in group_runs(rows):
        if not predicate(run) or str(run[-1]["scene"]) != scene:
            continue
        commits[str(run[-1]["machine"])] = str(run[-1]["commit"])
    return commits


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


def render_chart(
    rows: list[dict[str, object]], commit: str | None, scene: str
) -> str:
    selected = [
        row
        for row in rows
        if (commit is None or row["commit"] == commit)
        and row["threads"] in THREAD_COUNTS
    ]
    if not selected:
        return placeholder(f"No {scene} benchmark results for commit {commit}")

    # Later files replace earlier runs for the same machine/scenario point.
    points: dict[tuple[str, int, int], float] = {}
    for row in selected:
        points[(str(row["machine"]), int(row["threads"]), int(row["particles"]))] = float(row["average_ms"])

    machines = sorted({key[0] for key in points})
    cpu_labels = cpu_labels_by_machine(selected, machines)
    machines.sort(key=lambda machine: cpu_labels[machine])
    machine_commits = {
        machine: str(next(
            row["commit"]
            for row in reversed(selected)
            if str(row["machine"]) == machine
        ))
        for machine in machines
    }
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

    subtitle = (
        f"scene: {scene} · commit: {commit}"
        if commit is not None
        else f"scene: {scene} · latest compatible run per CPU"
    )
    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#101418"/>',
        text(width / 2, 34, "Simulation update time", fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 26, "font-family": "sans-serif"}),
        text(width / 2, 61, subtitle, fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}),
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
        svg.append(text(x0 + panel_width / 2, margin_top - 28, cpu_labels[machine], fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 16, "font-family": "sans-serif"}))
        svg.append(text(x0 + panel_width / 2, margin_top - 10, machine_commits[machine], fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 12, "font-family": "sans-serif"}))

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
    rows: list[dict[str, object]], commit: str | None
) -> list[int]:
    threads_by_particle: dict[int, set[int]] = {}
    for row in rows:
        if commit is not None and row["commit"] != commit:
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
    commit: str | None,
    scene: str,
    requested_particle_counts: set[int] | None = None,
) -> str:
    selected = [
        row
        for row in rows
        if commit is None or row["commit"] == commit
    ]
    if not selected:
        return placeholder(
            "No matching thread-scaling results"
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

    # Later files replace earlier runs for the same machine/scenario point.
    all_points: dict[tuple[str, int, int], float] = {}
    for row in selected:
        particles = int(row["particles"])
        all_points[(
            str(row["machine"]),
            particles,
            int(row["threads"]),
        )] = float(row["average_ms"])

    raw_points = {
        key: value
        for key, value in all_points.items()
        if key[1] in particle_counts
    }

    machines = sorted({str(row["machine"]) for row in selected})
    reference_times = {
        machine: all_points[(
            machine,
            THREAD_SCALING_REFERENCE_PARTICLES,
            1,
        )]
        for machine in machines
        if (
            machine,
            THREAD_SCALING_REFERENCE_PARTICLES,
            1,
        ) in all_points
    }

    points = {
        key: value / reference_times[key[0]]
        for key, value in raw_points.items()
        if key[0] in reference_times
    }

    machines = sorted({key[0] for key in points})
    if not machines:
        return placeholder("A 32K / 1-thread reference is required per CPU")
    cpu_labels = cpu_labels_by_machine(selected, machines)
    machines.sort(key=lambda machine: cpu_labels[machine])
    machine_commits = {
        machine: str(next(
            row["commit"]
            for row in reversed(selected)
            if str(row["machine"]) == machine
        ))
        for machine in machines
    }
    if any(
        len({key[2] for key in points if key[0] == machine}) < 2
        for machine in machines
    ):
        return placeholder("Not enough thread-scaling points to plot")

    width = 1400
    height = 760
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

    subtitle = (
        f"scene: {scene} · commit: {commit}"
        if commit is not None
        else f"scene: {scene} · latest compatible run per CPU"
    )
    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#101418"/>',
        text(width / 2, 34, "Thread scaling relative to each CPU's 32K baseline", fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 26, "font-family": "sans-serif"}),
        text(width / 2, 61, subtitle, fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}),
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
        machine_thread_counts = sorted({
            key[2] for key in points if key[0] == machine
        })
        machine_values = [
            value for key, value in points.items() if key[0] == machine
        ]
        # Keep major grid lines on integer values and leave a little headroom.
        y_max = max(1, math.ceil(max(machine_values) * 1.05))
        thread_min = min(machine_thread_counts)
        thread_max = max(machine_thread_counts)
        thread_range = max(1, thread_max - thread_min)
        tick_step = max(1, math.ceil(thread_max / 12))
        svg.append(f'<rect x="{x0:.1f}" y="{margin_top}" width="{panel_width:.1f}" height="{plot_height}" fill="#171d23" stroke="#45515e"/>')
        svg.append(text(x0 + panel_width / 2, margin_top - 28, cpu_labels[machine], fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 16, "font-family": "sans-serif"}))
        svg.append(text(x0 + panel_width / 2, margin_top - 10, machine_commits[machine], fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 12, "font-family": "sans-serif"}))

        for tick in range(y_max * 5 + 1):
            value = tick / 5
            y = bottom - plot_height * value / y_max
            is_major = tick % 5 == 0
            grid_color = "#36424f" if is_major else "#252e37"
            grid_width = "1" if is_major else "0.6"
            svg.append(f'<line x1="{x0:.1f}" y1="{y:.1f}" x2="{x0 + panel_width:.1f}" y2="{y:.1f}" stroke="{grid_color}" stroke-width="{grid_width}"/>')
            if is_major:
                svg.append(text(x0 - 8, y + 5, str(tick // 5), fill="#aeb8c4", **{"text-anchor": "end", "font-size": 12, "font-family": "sans-serif"}))

        for threads in machine_thread_counts:
            if threads != thread_min and threads != thread_max and threads % tick_step:
                continue
            x = x0 + panel_width * (threads - thread_min) / thread_range
            svg.append(f'<line x1="{x:.1f}" y1="{bottom:.1f}" x2="{x:.1f}" y2="{bottom + 5:.1f}" stroke="#aeb8c4"/>')
            svg.append(text(x, bottom + 24, str(threads), fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 12, "font-family": "sans-serif"}))

        for particle_index, particles in enumerate(particle_counts):
            color = COLORS[particle_index % len(COLORS)]
            machine_points = [
                (threads, points[(machine, particles, threads)])
                for threads in machine_thread_counts
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
                svg.append(f'<polyline points="{joined}" fill="none" stroke="{color}" stroke-width="2.5"/>')
            for x, y in coordinates:
                svg.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.5" fill="{color}"/>')

    svg.append(text(width / 2, height - 22, "Worker threads", fill="#d8dee9", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}))
    svg.append(text(20, margin_top + plot_height / 2, "Relative update time (32K / 1 thread = 1)", fill="#d8dee9", transform=f"rotate(-90 20 {margin_top + plot_height / 2:.1f})", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}))

    svg.append("</svg>")
    return "\n".join(svg)


def render_thread_log_chart(
    rows: list[dict[str, object]],
    commit: str | None,
    scene: str,
    requested_particle_counts: set[int] | None = None,
) -> str:
    selected = [
        row
        for row in rows
        if commit is None or row["commit"] == commit
    ]
    if not selected:
        return placeholder("No matching thread-scaling results")

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

    all_points: dict[tuple[str, int, int], float] = {}
    for row in selected:
        all_points[(
            str(row["machine"]),
            int(row["particles"]),
            int(row["threads"]),
        )] = float(row["average_ms"])

    machines = sorted({str(row["machine"]) for row in selected})
    reference_times = {
        machine: all_points[(
            machine,
            THREAD_SCALING_REFERENCE_PARTICLES,
            1,
        )]
        for machine in machines
        if (
            machine,
            THREAD_SCALING_REFERENCE_PARTICLES,
            1,
        ) in all_points
    }
    points = {
        key: value / reference_times[key[0]]
        for key, value in all_points.items()
        if key[0] in reference_times and key[1] in particle_counts
    }

    machines = sorted({key[0] for key in points})
    if not machines:
        return placeholder("A 32K / 1-thread reference is required per CPU")
    cpu_labels = cpu_labels_by_machine(selected, machines)
    machines.sort(key=lambda machine: cpu_labels[machine])
    machine_commits = {
        machine: str(next(
            row["commit"]
            for row in reversed(selected)
            if str(row["machine"]) == machine
        ))
        for machine in machines
    }

    positive_values = [value for value in points.values() if value > 0]
    if not positive_values:
        return placeholder("Thread-scaling times must be positive")
    y_min = 2 ** math.floor(math.log2(min(positive_values) * 0.9))
    y_max = 2 ** math.ceil(math.log2(max(positive_values) * 1.05))
    if y_min == y_max:
        y_min /= 2
        y_max *= 2
    log_y_min = math.log2(y_min)
    log_y_range = math.log2(y_max) - log_y_min

    width = 1400
    height = 760
    margin_left = 90
    margin_right = 38
    margin_top = 135
    margin_bottom = 82
    panel_gap = 55
    panel_width = (
        width - margin_left - margin_right - panel_gap * (len(machines) - 1)
    ) / len(machines)
    plot_height = height - margin_top - margin_bottom
    bottom = margin_top + plot_height

    subtitle = (
        f"scene: {scene} · commit: {commit}"
        if commit is not None
        else f"scene: {scene} · latest compatible run per CPU"
    )
    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#101418"/>',
        text(width / 2, 34, "Thread scaling on logarithmic axes", fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 26, "font-family": "sans-serif"}),
        text(width / 2, 61, subtitle, fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}),
    ]

    legend_width = len(particle_counts) * 115 + 190
    legend_x = (width - legend_width) / 2
    for particle_index, particles in enumerate(particle_counts):
        color = COLORS[particle_index % len(COLORS)]
        x = legend_x + particle_index * 115
        svg.append(f'<line x1="{x:.1f}" y1="88" x2="{x + 25:.1f}" y2="88" stroke="{color}" stroke-width="2.5"/>')
        svg.append(text(x + 33, 93, f"{particles // 1024}K", fill="#d8dee9", **{"font-size": 13, "font-family": "sans-serif"}))
    ideal_x = legend_x + len(particle_counts) * 115 + 5
    svg.append(f'<line x1="{ideal_x:.1f}" y1="88" x2="{ideal_x + 30:.1f}" y2="88" stroke="#aeb8c4" stroke-width="1.5" stroke-dasharray="6 5"/>')
    svg.append(text(ideal_x + 38, 93, "ideal 1 / threads", fill="#d8dee9", **{"font-size": 13, "font-family": "sans-serif"}))

    for machine_index, machine in enumerate(machines):
        x0 = margin_left + machine_index * (panel_width + panel_gap)
        machine_thread_counts = sorted({
            key[2] for key in points if key[0] == machine
        })
        thread_min = min(machine_thread_counts)
        thread_max = max(machine_thread_counts)
        log_x_min = math.log2(thread_min)
        log_x_range = max(1.0, math.log2(thread_max) - log_x_min)

        def x_position(threads: int) -> float:
            return x0 + panel_width * (
                math.log2(threads) - log_x_min
            ) / log_x_range

        def y_position(value: float) -> float:
            return bottom - plot_height * (
                math.log2(value) - log_y_min
            ) / log_y_range

        clip_id = f"log-panel-{machine_index}"
        svg.append(
            f'<defs><clipPath id="{clip_id}"><rect x="{x0:.1f}" '
            f'y="{margin_top}" width="{panel_width:.1f}" '
            f'height="{plot_height}"/></clipPath></defs>'
        )
        svg.append(f'<rect x="{x0:.1f}" y="{margin_top}" width="{panel_width:.1f}" height="{plot_height}" fill="#171d23" stroke="#45515e"/>')
        svg.append(text(x0 + panel_width / 2, margin_top - 28, cpu_labels[machine], fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 16, "font-family": "sans-serif"}))
        svg.append(text(x0 + panel_width / 2, margin_top - 10, machine_commits[machine], fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 12, "font-family": "sans-serif"}))

        y_power = math.ceil(log_y_min)
        while y_power <= math.floor(math.log2(y_max)):
            value = 2 ** y_power
            y = y_position(value)
            is_baseline = value == 1
            grid_color = "#667482" if is_baseline else "#2b3540"
            grid_width = "1.5" if is_baseline else "1"
            svg.append(f'<line x1="{x0:.1f}" y1="{y:.1f}" x2="{x0 + panel_width:.1f}" y2="{y:.1f}" stroke="{grid_color}" stroke-width="{grid_width}"/>')
            svg.append(text(x0 - 8, y + 5, f"{value:g}", fill="#aeb8c4", **{"text-anchor": "end", "font-size": 12, "font-family": "sans-serif"}))
            y_power += 1

        x_ticks: list[int] = []
        power = 1
        while power <= thread_max:
            if power >= thread_min:
                x_ticks.append(power)
            power *= 2
        if thread_max not in x_ticks:
            x_ticks.append(thread_max)
        for threads in x_ticks:
            x = x_position(threads)
            svg.append(f'<line x1="{x:.1f}" y1="{margin_top}" x2="{x:.1f}" y2="{bottom:.1f}" stroke="#252e37" stroke-width="0.6"/>')
            svg.append(f'<line x1="{x:.1f}" y1="{bottom:.1f}" x2="{x:.1f}" y2="{bottom + 5:.1f}" stroke="#aeb8c4"/>')
            svg.append(text(x, bottom + 24, str(threads), fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 12, "font-family": "sans-serif"}))

        for particle_index, particles in enumerate(particle_counts):
            color = COLORS[particle_index % len(COLORS)]
            particle_ratio = particles / THREAD_SCALING_REFERENCE_PARTICLES
            ideal_coordinates = [
                (x_position(thread_min), y_position(particle_ratio / thread_min)),
                (x_position(thread_max), y_position(particle_ratio / thread_max)),
            ]
            ideal_joined = " ".join(
                f"{x:.1f},{y:.1f}" for x, y in ideal_coordinates
            )
            svg.append(f'<polyline points="{ideal_joined}" fill="none" stroke="{color}" stroke-width="1.5" stroke-opacity="0.45" stroke-dasharray="6 5" clip-path="url(#{clip_id})"/>')

            machine_points = sorted(
                (
                    threads,
                    points[(machine, particles, threads)],
                )
                for threads in machine_thread_counts
                if (machine, particles, threads) in points
            )
            coordinates = [
                (x_position(threads), y_position(value))
                for threads, value in machine_points
                if value > 0
            ]
            if len(coordinates) > 1:
                joined = " ".join(
                    f"{x:.1f},{y:.1f}" for x, y in coordinates
                )
                svg.append(f'<polyline points="{joined}" fill="none" stroke="{color}" stroke-width="2.5" clip-path="url(#{clip_id})"/>')
            for x, y in coordinates:
                svg.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.5" fill="{color}" clip-path="url(#{clip_id})"/>')

    svg.append(text(width / 2, height - 22, "Worker threads (log₂ scale)", fill="#d8dee9", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}))
    svg.append(text(20, margin_top + plot_height / 2, "Relative update time (log₂ scale)", fill="#d8dee9", transform=f"rotate(-90 20 {margin_top + plot_height / 2:.1f})", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}))

    svg.append("</svg>")
    return "\n".join(svg)


def render_thread_efficiency_chart(
    rows: list[dict[str, object]],
    commit: str | None,
    scene: str,
    requested_particle_counts: set[int] | None = None,
) -> str:
    selected = [
        row
        for row in rows
        if commit is None or row["commit"] == commit
    ]
    if not selected:
        return placeholder("No matching thread-scaling results")

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

    all_points: dict[tuple[str, int, int], float] = {}
    for row in selected:
        all_points[(
            str(row["machine"]),
            int(row["particles"]),
            int(row["threads"]),
        )] = float(row["average_ms"])

    single_thread_times = {
        (machine, particles): value
        for (machine, particles, threads), value in all_points.items()
        if threads == 1 and particles in particle_counts and value > 0
    }
    efficiency_points = {
        key: single_thread_times[(key[0], key[1])] / (value * key[2])
        for key, value in all_points.items()
        if key[1] in particle_counts
        and value > 0
        and (key[0], key[1]) in single_thread_times
    }

    machines = sorted({key[0] for key in efficiency_points})
    if not machines:
        return placeholder("A 1-thread reference is required per CPU and particle count")
    cpu_labels = cpu_labels_by_machine(selected, machines)
    machines.sort(key=lambda machine: cpu_labels[machine])
    machine_commits = {
        machine: str(next(
            row["commit"]
            for row in reversed(selected)
            if str(row["machine"]) == machine
        ))
        for machine in machines
    }

    max_efficiency = max(efficiency_points.values())
    y_max = max(1.0, math.ceil(max_efficiency * 1.05 * 10) / 10)
    y_steps = round(y_max * 10)

    width = 1400
    height = 620
    margin_left = 90
    margin_right = 38
    margin_top = 135
    margin_bottom = 78
    panel_gap = 55
    panel_width = (
        width - margin_left - margin_right - panel_gap * (len(machines) - 1)
    ) / len(machines)
    plot_height = height - margin_top - margin_bottom
    bottom = margin_top + plot_height

    subtitle = (
        f"scene: {scene} · commit: {commit}"
        if commit is not None
        else f"scene: {scene} · latest compatible run per CPU"
    )
    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#101418"/>',
        text(width / 2, 34, "Parallel efficiency per worker", fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 26, "font-family": "sans-serif"}),
        text(width / 2, 61, subtitle, fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}),
    ]

    legend_width = len(particle_counts) * 115 + 145
    legend_x = (width - legend_width) / 2
    for particle_index, particles in enumerate(particle_counts):
        color = COLORS[particle_index % len(COLORS)]
        x = legend_x + particle_index * 115
        svg.append(f'<line x1="{x:.1f}" y1="88" x2="{x + 25:.1f}" y2="88" stroke="{color}" stroke-width="2.5"/>')
        svg.append(text(x + 33, 93, f"{particles // 1024}K", fill="#d8dee9", **{"font-size": 13, "font-family": "sans-serif"}))
    ideal_x = legend_x + len(particle_counts) * 115 + 5
    svg.append(f'<line x1="{ideal_x:.1f}" y1="88" x2="{ideal_x + 30:.1f}" y2="88" stroke="#aeb8c4" stroke-width="1.5" stroke-dasharray="6 5"/>')
    svg.append(text(ideal_x + 38, 93, "100% ideal", fill="#d8dee9", **{"font-size": 13, "font-family": "sans-serif"}))

    for machine_index, machine in enumerate(machines):
        x0 = margin_left + machine_index * (panel_width + panel_gap)
        machine_thread_counts = sorted({
            key[2] for key in efficiency_points if key[0] == machine
        })
        thread_min = min(machine_thread_counts)
        thread_max = max(machine_thread_counts)
        thread_range = max(1, thread_max - thread_min)
        tick_step = max(1, math.ceil(thread_max / 12))

        svg.append(f'<rect x="{x0:.1f}" y="{margin_top}" width="{panel_width:.1f}" height="{plot_height}" fill="#171d23" stroke="#45515e"/>')
        svg.append(text(x0 + panel_width / 2, margin_top - 28, cpu_labels[machine], fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 16, "font-family": "sans-serif"}))
        svg.append(text(x0 + panel_width / 2, margin_top - 10, machine_commits[machine], fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 12, "font-family": "sans-serif"}))

        for tick in range(y_steps + 1):
            value = tick / 10
            y = bottom - plot_height * value / y_max
            is_major = tick % 2 == 0
            is_ideal = tick == 10
            grid_color = (
                "#667482" if is_ideal
                else "#36424f" if is_major
                else "#252e37"
            )
            grid_width = "1.5" if is_ideal else "1" if is_major else "0.6"
            dash = ' stroke-dasharray="6 5"' if is_ideal else ""
            svg.append(f'<line x1="{x0:.1f}" y1="{y:.1f}" x2="{x0 + panel_width:.1f}" y2="{y:.1f}" stroke="{grid_color}" stroke-width="{grid_width}"{dash}/>')
            if is_major or is_ideal:
                svg.append(text(x0 - 8, y + 5, f"{round(value * 100):d}%", fill="#aeb8c4", **{"text-anchor": "end", "font-size": 12, "font-family": "sans-serif"}))

        for threads in machine_thread_counts:
            if threads != thread_min and threads != thread_max and threads % tick_step:
                continue
            x = x0 + panel_width * (threads - thread_min) / thread_range
            svg.append(f'<line x1="{x:.1f}" y1="{bottom:.1f}" x2="{x:.1f}" y2="{bottom + 5:.1f}" stroke="#aeb8c4"/>')
            svg.append(text(x, bottom + 24, str(threads), fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 12, "font-family": "sans-serif"}))

        for particle_index, particles in enumerate(particle_counts):
            color = COLORS[particle_index % len(COLORS)]
            machine_points = sorted(
                (
                    threads,
                    efficiency_points[(machine, particles, threads)],
                )
                for threads in machine_thread_counts
                if (machine, particles, threads) in efficiency_points
            )
            coordinates = [
                (
                    x0 + panel_width * (threads - thread_min) / thread_range,
                    bottom - plot_height * value / y_max,
                )
                for threads, value in machine_points
            ]
            if len(coordinates) > 1:
                joined = " ".join(
                    f"{x:.1f},{y:.1f}" for x, y in coordinates
                )
                svg.append(f'<polyline points="{joined}" fill="none" stroke="{color}" stroke-width="2.5"/>')
            for x, y in coordinates:
                svg.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.5" fill="{color}"/>')

    svg.append(text(width / 2, height - 22, "Worker threads", fill="#d8dee9", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}))
    svg.append(text(20, margin_top + plot_height / 2, "Useful throughput per worker", fill="#d8dee9", transform=f"rotate(-90 20 {margin_top + plot_height / 2:.1f})", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}))

    svg.append("</svg>")
    return "\n".join(svg)


def render_machine_comparison_chart(
    rows: list[dict[str, object]],
    commit: str | None,
    scene: str,
    requested_particle_counts: set[int] | None = None,
) -> str:
    selected = [
        row
        for row in rows
        if commit is None or row["commit"] == commit
    ]
    if not selected:
        return placeholder("No matching thread-scaling results")

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

    # Later files replace earlier runs for the same machine/scenario point.
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
    if not machines:
        return placeholder("No machine-comparison points to plot")
    cpu_labels = cpu_labels_by_machine(selected, machines)
    machines.sort(key=lambda machine: cpu_labels[machine])
    machine_commits = {
        machine: str(next(
            row["commit"]
            for row in reversed(selected)
            if str(row["machine"]) == machine
        ))
        for machine in machines
    }

    all_thread_counts = sorted({key[2] for key in points})
    thread_min = min(all_thread_counts)
    thread_max = max(all_thread_counts)
    log_x_min = math.log2(thread_min)
    log_x_range = max(1.0, math.log2(thread_max) - log_x_min)

    width = 1400
    height = 650
    margin_left = 90
    margin_right = 38
    margin_top = 165
    margin_bottom = 82
    panel_gap = 55
    panel_width = (
        width - margin_left - margin_right
        - panel_gap * (len(particle_counts) - 1)
    ) / len(particle_counts)
    plot_height = height - margin_top - margin_bottom
    bottom = margin_top + plot_height

    subtitle = (
        f"scene: {scene} · commit: {commit}"
        if commit is not None
        else f"scene: {scene} · latest compatible run per CPU"
    )
    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#101418"/>',
        text(width / 2, 34, "CPU comparison by particle load", fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 26, "font-family": "sans-serif"}),
        text(width / 2, 61, subtitle, fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}),
    ]

    legend_width = width - 120
    legend_item_width = legend_width / len(machines)
    legend_x = 60
    for machine_index, machine in enumerate(machines):
        color = COLORS[machine_index % len(COLORS)]
        x = legend_x + machine_index * legend_item_width
        svg.append(f'<line x1="{x:.1f}" y1="91" x2="{x + 25:.1f}" y2="91" stroke="{color}" stroke-width="2.5"/>')
        svg.append(text(x + 33, 96, cpu_labels[machine], fill="#d8dee9", **{"font-size": 13, "font-family": "sans-serif"}))
        svg.append(text(x + 33, 113, machine_commits[machine], fill="#8995a3", **{"font-size": 11, "font-family": "sans-serif"}))

    x_ticks: list[int] = []
    power = 1
    while power <= thread_max:
        if power >= thread_min:
            x_ticks.append(power)
        power *= 2
    if thread_max not in x_ticks:
        x_ticks.append(thread_max)

    for particle_index, particles in enumerate(particle_counts):
        x0 = margin_left + particle_index * (panel_width + panel_gap)
        panel_values = [
            value
            for (machine, point_particles, threads), value in points.items()
            if point_particles == particles
        ]
        max_value = max(panel_values)
        raw_step = max_value * 1.05 / 6
        magnitude = 10 ** math.floor(math.log10(raw_step))
        residual = raw_step / magnitude
        if residual <= 1:
            tick_step = magnitude
        elif residual <= 2:
            tick_step = 2 * magnitude
        elif residual <= 5:
            tick_step = 5 * magnitude
        else:
            tick_step = 10 * magnitude
        y_max = math.ceil(max_value * 1.05 / tick_step) * tick_step
        y_tick_count = round(y_max / tick_step)

        def x_position(threads: int) -> float:
            return x0 + panel_width * (
                math.log2(threads) - log_x_min
            ) / log_x_range

        def y_position(value: float) -> float:
            return bottom - plot_height * value / y_max

        svg.append(f'<rect x="{x0:.1f}" y="{margin_top}" width="{panel_width:.1f}" height="{plot_height}" fill="#171d23" stroke="#45515e"/>')
        svg.append(text(x0 + panel_width / 2, margin_top - 13, f"{particles // 1024}K particles", fill="#f2f4f8", **{"text-anchor": "middle", "font-size": 16, "font-family": "sans-serif"}))

        decimals = 0 if tick_step >= 1 else 1
        for tick in range(y_tick_count + 1):
            value = tick * tick_step
            y = y_position(value)
            svg.append(f'<line x1="{x0:.1f}" y1="{y:.1f}" x2="{x0 + panel_width:.1f}" y2="{y:.1f}" stroke="#2b3540"/>')
            svg.append(text(x0 - 8, y + 5, f"{value:.{decimals}f}", fill="#aeb8c4", **{"text-anchor": "end", "font-size": 12, "font-family": "sans-serif"}))

        for threads in x_ticks:
            x = x_position(threads)
            svg.append(f'<line x1="{x:.1f}" y1="{margin_top}" x2="{x:.1f}" y2="{bottom:.1f}" stroke="#252e37" stroke-width="0.6"/>')
            svg.append(f'<line x1="{x:.1f}" y1="{bottom:.1f}" x2="{x:.1f}" y2="{bottom + 5:.1f}" stroke="#aeb8c4"/>')
            svg.append(text(x, bottom + 24, str(threads), fill="#aeb8c4", **{"text-anchor": "middle", "font-size": 12, "font-family": "sans-serif"}))

        for machine_index, machine in enumerate(machines):
            color = COLORS[machine_index % len(COLORS)]
            machine_points = sorted(
                (threads, points[(machine, particles, threads)])
                for threads in all_thread_counts
                if (machine, particles, threads) in points
            )
            coordinates = [
                (x_position(threads), y_position(value))
                for threads, value in machine_points
            ]
            if len(coordinates) > 1:
                joined = " ".join(
                    f"{x:.1f},{y:.1f}" for x, y in coordinates
                )
                svg.append(f'<polyline points="{joined}" fill="none" stroke="{color}" stroke-width="2.5"/>')
            for x, y in coordinates:
                svg.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.5" fill="{color}"/>')

    svg.append(text(width / 2, height - 22, "Worker threads (log₂ scale)", fill="#d8dee9", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}))
    svg.append(text(20, margin_top + plot_height / 2, "Average update time (ms)", fill="#d8dee9", transform=f"rotate(-90 20 {margin_top + plot_height / 2:.1f})", **{"text-anchor": "middle", "font-size": 15, "font-family": "sans-serif"}))

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
    parser.add_argument(
        "--thread-log-output",
        type=Path,
        default=Path("results/benchmark_thread_scaling_log.svg"),
    )
    parser.add_argument(
        "--thread-efficiency-output",
        type=Path,
        default=Path("results/benchmark_thread_efficiency.svg"),
    )
    parser.add_argument(
        "--machine-comparison-output",
        type=Path,
        default=Path("results/benchmark_machine_comparison.svg"),
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
    all_particle_rows = rows_for_run_type(all_rows, "particle")
    if args.commit is not None:
        particle_commit: str | None = args.commit
        particle_rows = [
            row
            for row in all_particle_rows
            if row["scene"] == particle_scene
            and row["commit"] == particle_commit
        ]
    else:
        particle_commit = None
        latest_particle_commits = latest_commit_per_machine(
            all_rows, "particle", particle_scene
        )
        particle_rows = [
            row
            for row in all_particle_rows
            if row["scene"] == particle_scene
            and row["commit"] == latest_particle_commits.get(
                str(row["machine"])
            )
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
    all_thread_rows = rows_for_run_type(all_rows, "thread")
    if args.commit is not None:
        thread_commit: str | None = args.commit
        thread_rows = [
            row
            for row in all_thread_rows
            if row["scene"] == thread_scene
            and row["commit"] == thread_commit
        ]
    else:
        thread_commit = None
        latest_commits = latest_commit_per_machine(
            all_rows, "thread", thread_scene
        )
        thread_rows = [
            row
            for row in all_thread_rows
            if row["scene"] == thread_scene
            and row["commit"] == latest_commits.get(str(row["machine"]))
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

    thread_log_chart = (
        render_thread_log_chart(
            thread_rows,
            thread_commit,
            thread_scene,
            requested_thread_particles,
        )
        if thread_rows and available_thread_particles
        else placeholder("Run the thread-scaling benchmark to generate this chart")
    )
    args.thread_log_output.parent.mkdir(parents=True, exist_ok=True)
    args.thread_log_output.write_text(thread_log_chart, encoding="utf-8")
    print(f"Wrote {args.thread_log_output}")

    thread_efficiency_chart = (
        render_thread_efficiency_chart(
            thread_rows,
            thread_commit,
            thread_scene,
            requested_thread_particles,
        )
        if thread_rows and available_thread_particles
        else placeholder("Run the thread-scaling benchmark to generate this chart")
    )
    args.thread_efficiency_output.parent.mkdir(parents=True, exist_ok=True)
    args.thread_efficiency_output.write_text(
        thread_efficiency_chart,
        encoding="utf-8",
    )
    print(f"Wrote {args.thread_efficiency_output}")

    machine_comparison_chart = (
        render_machine_comparison_chart(
            thread_rows,
            thread_commit,
            thread_scene,
            requested_thread_particles,
        )
        if thread_rows and available_thread_particles
        else placeholder("Run the thread-scaling benchmark to generate this chart")
    )
    args.machine_comparison_output.parent.mkdir(parents=True, exist_ok=True)
    args.machine_comparison_output.write_text(
        machine_comparison_chart,
        encoding="utf-8",
    )
    print(f"Wrote {args.machine_comparison_output}")


if __name__ == "__main__":
    main()
