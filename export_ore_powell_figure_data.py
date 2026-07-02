#!/usr/bin/env python3
"""
Export Excel-ready CSV files from an independently validated
Ore-Powell reference run.

Inputs
------
1. annihilation_gammas.csv
2. ore_powell_reference_grid.csv
3. ore_powell_reference_summary.json

Outputs
-------
phase_space_long.csv
observed_density_matrix.csv
expected_density_matrix.csv
residual_matrix.csv
single_photon_spectrum.csv
ordered_energy_spectra.csv
opening_angle_spectra.csv
validation_summary.csv
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import shutil
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any, Iterable

import numpy as np


ELECTRON_MASS_MEV = 0.51099895

REQUIRED_GAMMA_COLUMNS = {
    "event_id",
    "kinetic_energy_MeV",
    "dir_x",
    "dir_y",
    "dir_z",
}

REQUIRED_GRID_COLUMNS = {
    "r1_low",
    "r1_high",
    "r2_low",
    "r2_high",
    "observed_count",
    "expected_probability",
    "expected_count",
    "standardized_residual",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Export Excel-ready Ore-Powell validation data."
    )

    parser.add_argument(
        "--gammas",
        type=Path,
        required=True,
        help="Path to annihilation_gammas.csv.",
    )

    parser.add_argument(
        "--grid",
        type=Path,
        required=True,
        help="Path to ore_powell_reference_grid.csv.",
    )

    parser.add_argument(
        "--summary",
        type=Path,
        required=True,
        help="Path to ore_powell_reference_summary.json.",
    )

    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("ore_powell_figure_data"),
        help="Output directory.",
    )

    parser.add_argument(
        "--energy-bins",
        type=int,
        default=50,
        help="Number of bins for normalized-energy spectra.",
    )

    parser.add_argument(
        "--angle-bins",
        type=int,
        default=90,
        help="Number of bins from 0 to 180 degrees.",
    )

    return parser.parse_args()


def require_file(path: Path) -> None:
    if not path.is_file():
        raise FileNotFoundError(f"Required file not found: {path}")


def parse_float(text: str) -> float:
    value = text.strip()

    if value == "":
        return math.nan

    try:
        return float(value)
    except ValueError as exc:
        raise ValueError(f"Could not parse floating-point value: {text!r}") from exc


def read_grid(path: Path) -> list[dict[str, float]]:
    rows: list[dict[str, float]] = []

    with path.open("r", newline="") as handle:
        reader = csv.DictReader(handle)
        fieldnames = set(reader.fieldnames or [])
        missing = REQUIRED_GRID_COLUMNS - fieldnames

        if missing:
            raise ValueError(
                f"{path}: missing required columns: {sorted(missing)}"
            )

        for row in reader:
            rows.append(
                {
                    "r1_low": parse_float(row["r1_low"]),
                    "r1_high": parse_float(row["r1_high"]),
                    "r2_low": parse_float(row["r2_low"]),
                    "r2_high": parse_float(row["r2_high"]),
                    "observed_count": parse_float(row["observed_count"]),
                    "expected_probability": parse_float(
                        row["expected_probability"]
                    ),
                    "expected_count": parse_float(row["expected_count"]),
                    "standardized_residual": parse_float(
                        row["standardized_residual"]
                    ),
                }
            )

    if not rows:
        raise ValueError(f"{path}: grid CSV contains no data rows")

    return rows


def read_gamma_events(
    path: Path,
) -> dict[int, list[tuple[float, np.ndarray]]]:
    events: dict[int, list[tuple[float, np.ndarray]]] = defaultdict(list)

    with path.open("r", newline="") as handle:
        reader = csv.DictReader(handle)
        fieldnames = set(reader.fieldnames or [])
        missing = REQUIRED_GAMMA_COLUMNS - fieldnames

        if missing:
            raise ValueError(
                f"{path}: missing required columns: {sorted(missing)}"
            )

        for row_number, row in enumerate(reader, start=2):
            event_id = int(row["event_id"])
            energy_mev = float(row["kinetic_energy_MeV"])

            direction = np.asarray(
                [
                    float(row["dir_x"]),
                    float(row["dir_y"]),
                    float(row["dir_z"]),
                ],
                dtype=float,
            )

            norm = float(np.linalg.norm(direction))

            if not math.isfinite(norm) or norm <= 0.0:
                raise ValueError(
                    f"{path}:{row_number}: invalid direction norm {norm}"
                )

            direction /= norm
            events[event_id].append((energy_mev, direction))

    if not events:
        raise ValueError(f"{path}: gamma CSV contains no events")

    return dict(events)


def validate_three_gamma_events(
    events: dict[int, list[tuple[float, np.ndarray]]],
) -> None:
    failures = [
        event_id
        for event_id, photons in events.items()
        if len(photons) != 3
    ]

    if failures:
        preview = ", ".join(str(value) for value in failures[:10])
        raise ValueError(
            f"{len(failures)} events do not contain exactly three photons. "
            f"First event IDs: {preview}"
        )


def write_phase_space_long(
    rows: list[dict[str, float]],
    event_count: int,
    output_path: Path,
) -> None:
    fieldnames = [
        "r1_low",
        "r1_high",
        "r1_center",
        "r2_low",
        "r2_high",
        "r2_center",
        "bin_area",
        "observed_count",
        "observed_probability",
        "observed_density",
        "expected_probability",
        "expected_density",
        "expected_count",
        "standardized_residual",
    ]

    with output_path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()

        for row in rows:
            r1_width = row["r1_high"] - row["r1_low"]
            r2_width = row["r2_high"] - row["r2_low"]
            bin_area = r1_width * r2_width

            observed_probability = (
                row["observed_count"] / event_count
                if event_count > 0
                else math.nan
            )

            observed_density = (
                observed_probability / bin_area
                if bin_area > 0.0
                else math.nan
            )

            expected_density = (
                row["expected_probability"] / bin_area
                if bin_area > 0.0
                else math.nan
            )

            residual = row["standardized_residual"]

            writer.writerow(
                {
                    "r1_low": row["r1_low"],
                    "r1_high": row["r1_high"],
                    "r1_center": 0.5
                    * (row["r1_low"] + row["r1_high"]),
                    "r2_low": row["r2_low"],
                    "r2_high": row["r2_high"],
                    "r2_center": 0.5
                    * (row["r2_low"] + row["r2_high"]),
                    "bin_area": bin_area,
                    "observed_count": int(row["observed_count"]),
                    "observed_probability": observed_probability,
                    "observed_density": observed_density,
                    "expected_probability": row["expected_probability"],
                    "expected_density": expected_density,
                    "expected_count": row["expected_count"],
                    "standardized_residual": (
                        "" if not math.isfinite(residual) else residual
                    ),
                }
            )


def grid_axes(
    rows: list[dict[str, float]],
) -> tuple[list[float], list[float]]:
    r1_centers = sorted(
        {
            0.5 * (row["r1_low"] + row["r1_high"])
            for row in rows
        }
    )

    r2_centers = sorted(
        {
            0.5 * (row["r2_low"] + row["r2_high"])
            for row in rows
        }
    )

    return r1_centers, r2_centers


def matrix_from_grid(
    rows: list[dict[str, float]],
    value_name: str,
    event_count: int,
) -> tuple[list[float], list[float], np.ndarray]:
    r1_centers, r2_centers = grid_axes(rows)

    r1_index = {
        round(value, 12): index
        for index, value in enumerate(r1_centers)
    }

    r2_index = {
        round(value, 12): index
        for index, value in enumerate(r2_centers)
    }

    matrix = np.full(
        (len(r2_centers), len(r1_centers)),
        np.nan,
        dtype=float,
    )

    for row in rows:
        r1_center = 0.5 * (row["r1_low"] + row["r1_high"])
        r2_center = 0.5 * (row["r2_low"] + row["r2_high"])

        i = r2_index[round(r2_center, 12)]
        j = r1_index[round(r1_center, 12)]

        bin_area = (
            (row["r1_high"] - row["r1_low"])
            * (row["r2_high"] - row["r2_low"])
        )

        if value_name == "observed_density":
            if row["expected_probability"] <= 0.0:
                value = math.nan
            else:
                value = (
                    row["observed_count"] / event_count / bin_area
                )

        elif value_name == "expected_density":
            if row["expected_probability"] <= 0.0:
                value = math.nan
            else:
                value = row["expected_probability"] / bin_area

        elif value_name == "standardized_residual":
            value = row["standardized_residual"]

        else:
            raise ValueError(f"Unknown grid value: {value_name}")

        matrix[i, j] = value

    return r1_centers, r2_centers, matrix


def write_matrix_csv(
    output_path: Path,
    r1_centers: list[float],
    r2_centers: list[float],
    matrix: np.ndarray,
) -> None:
    if matrix.shape != (len(r2_centers), len(r1_centers)):
        raise ValueError("Matrix dimensions do not match grid axes")

    with output_path.open("w", newline="") as handle:
        writer = csv.writer(handle)

        writer.writerow(
            ["r2_center"]
            + [f"{value:.8f}" for value in r1_centers]
        )

        for r2_value, row_values in zip(r2_centers, matrix):
            output_row: list[Any] = [f"{r2_value:.8f}"]

            for value in row_values:
                if math.isfinite(float(value)):
                    output_row.append(float(value))
                else:
                    output_row.append("")

            writer.writerow(output_row)


def histogram(
    values: Iterable[float],
    edges: np.ndarray,
) -> tuple[np.ndarray, np.ndarray]:
    values_array = np.asarray(list(values), dtype=float)
    counts, _ = np.histogram(values_array, bins=edges)

    total = counts.sum()

    if total > 0:
        probabilities = counts.astype(float) / float(total)
    else:
        probabilities = np.zeros_like(counts, dtype=float)

    return counts, probabilities


def expected_single_photon_probability(
    grid_rows: list[dict[str, float]],
    spectrum_edges: np.ndarray,
) -> np.ndarray:
    """
    Construct the one-photon marginal from the independently integrated
    two-dimensional expected grid.

    Because the Ore-Powell density is symmetric under photon exchange,
    each physical event contributes three marginal photon energies:
    r1, r2, and r3 = 2 - r1 - r2.

    Grid-bin probability is assigned to the bin center for this
    publication export. With the default 50-bin grid and 50-bin
    marginal spectrum, this is consistent with the grid resolution.
    """
    expected = np.zeros(len(spectrum_edges) - 1, dtype=float)

    for row in grid_rows:
        probability = row["expected_probability"]

        if probability <= 0.0:
            continue

        r1 = 0.5 * (row["r1_low"] + row["r1_high"])
        r2 = 0.5 * (row["r2_low"] + row["r2_high"])
        r3 = 2.0 - r1 - r2

        for value in (r1, r2, r3):
            index = int(
                np.searchsorted(
                    spectrum_edges,
                    value,
                    side="right",
                )
                - 1
            )

            if value == spectrum_edges[-1]:
                index = len(expected) - 1

            if 0 <= index < len(expected):
                expected[index] += probability / 3.0

    total = expected.sum()

    if total <= 0.0:
        raise ValueError(
            "Expected marginal spectrum has zero total probability"
        )

    return expected / total


def write_single_photon_spectrum(
    events: dict[int, list[tuple[float, np.ndarray]]],
    grid_rows: list[dict[str, float]],
    output_path: Path,
    bin_count: int,
) -> None:
    all_x = [
        energy_mev / ELECTRON_MASS_MEV
        for photons in events.values()
        for energy_mev, _ in photons
    ]

    edges = np.linspace(0.0, 1.0, bin_count + 1)
    widths = np.diff(edges)

    observed_count, observed_probability = histogram(all_x, edges)
    expected_probability = expected_single_photon_probability(
        grid_rows,
        edges,
    )

    observed_density = observed_probability / widths
    expected_density = expected_probability / widths

    with output_path.open("w", newline="") as handle:
        writer = csv.writer(handle)

        writer.writerow(
            [
                "x_low",
                "x_high",
                "x_center",
                "observed_count",
                "observed_probability",
                "observed_density",
                "expected_probability",
                "expected_density",
                "observed_cdf",
                "expected_cdf_grid_derived",
                "cdf_difference_grid_derived",
            ]
        )

        observed_cdf = np.cumsum(observed_probability)
        expected_cdf = np.cumsum(expected_probability)

        for index in range(bin_count):
            writer.writerow(
                [
                    edges[index],
                    edges[index + 1],
                    0.5 * (edges[index] + edges[index + 1]),
                    int(observed_count[index]),
                    observed_probability[index],
                    observed_density[index],
                    expected_probability[index],
                    expected_density[index],
                    observed_cdf[index],
                    expected_cdf[index],
                    observed_cdf[index] - expected_cdf[index],
                ]
            )


def write_ordered_energy_spectra(
    events: dict[int, list[tuple[float, np.ndarray]]],
    output_path: Path,
    bin_count: int,
) -> None:
    low_x: list[float] = []
    middle_x: list[float] = []
    high_x: list[float] = []

    for photons in events.values():
        ordered = sorted(
            energy_mev / ELECTRON_MASS_MEV
            for energy_mev, _ in photons
        )

        low_x.append(ordered[0])
        middle_x.append(ordered[1])
        high_x.append(ordered[2])

    edges = np.linspace(0.0, 1.0, bin_count + 1)
    widths = np.diff(edges)

    low_count, low_probability = histogram(low_x, edges)
    middle_count, middle_probability = histogram(middle_x, edges)
    high_count, high_probability = histogram(high_x, edges)

    with output_path.open("w", newline="") as handle:
        writer = csv.writer(handle)

        writer.writerow(
            [
                "x_low",
                "x_high",
                "x_center",
                "low_count",
                "low_probability",
                "low_density",
                "middle_count",
                "middle_probability",
                "middle_density",
                "high_count",
                "high_probability",
                "high_density",
            ]
        )

        for index in range(bin_count):
            writer.writerow(
                [
                    edges[index],
                    edges[index + 1],
                    0.5 * (edges[index] + edges[index + 1]),
                    int(low_count[index]),
                    low_probability[index],
                    low_probability[index] / widths[index],
                    int(middle_count[index]),
                    middle_probability[index],
                    middle_probability[index] / widths[index],
                    int(high_count[index]),
                    high_probability[index],
                    high_probability[index] / widths[index],
                ]
            )


def angle_degrees(
    direction_a: np.ndarray,
    direction_b: np.ndarray,
) -> float:
    cosine = float(np.dot(direction_a, direction_b))
    cosine = max(-1.0, min(1.0, cosine))
    return math.degrees(math.acos(cosine))


def write_opening_angle_spectra(
    events: dict[int, list[tuple[float, np.ndarray]]],
    output_path: Path,
    bin_count: int,
) -> None:
    low_middle: list[float] = []
    low_high: list[float] = []
    middle_high: list[float] = []
    all_pairs: list[float] = []

    for photons in events.values():
        ordered = sorted(photons, key=lambda item: item[0])

        low_direction = ordered[0][1]
        middle_direction = ordered[1][1]
        high_direction = ordered[2][1]

        angle_lm = angle_degrees(low_direction, middle_direction)
        angle_lh = angle_degrees(low_direction, high_direction)
        angle_mh = angle_degrees(middle_direction, high_direction)

        low_middle.append(angle_lm)
        low_high.append(angle_lh)
        middle_high.append(angle_mh)

        all_pairs.extend((angle_lm, angle_lh, angle_mh))

    edges = np.linspace(0.0, 180.0, bin_count + 1)
    widths = np.diff(edges)

    lm_count, lm_probability = histogram(low_middle, edges)
    lh_count, lh_probability = histogram(low_high, edges)
    mh_count, mh_probability = histogram(middle_high, edges)
    all_count, all_probability = histogram(all_pairs, edges)

    with output_path.open("w", newline="") as handle:
        writer = csv.writer(handle)

        writer.writerow(
            [
                "angle_low_deg",
                "angle_high_deg",
                "angle_center_deg",
                "low_middle_count",
                "low_middle_probability",
                "low_middle_density_per_degree",
                "low_high_count",
                "low_high_probability",
                "low_high_density_per_degree",
                "middle_high_count",
                "middle_high_probability",
                "middle_high_density_per_degree",
                "all_pairs_count",
                "all_pairs_probability",
                "all_pairs_density_per_degree",
            ]
        )

        for index in range(bin_count):
            writer.writerow(
                [
                    edges[index],
                    edges[index + 1],
                    0.5 * (edges[index] + edges[index + 1]),
                    int(lm_count[index]),
                    lm_probability[index],
                    lm_probability[index] / widths[index],
                    int(lh_count[index]),
                    lh_probability[index],
                    lh_probability[index] / widths[index],
                    int(mh_count[index]),
                    mh_probability[index],
                    mh_probability[index] / widths[index],
                    int(all_count[index]),
                    all_probability[index],
                    all_probability[index] / widths[index],
                ]
            )


def write_validation_summary(
    summary: dict[str, Any],
    output_path: Path,
) -> None:
    metrics = [
        ("Validation status", summary.get("status")),
        ("Events analyzed", summary.get("events_analyzed")),
        ("Multiplicity failures", summary.get("multiplicity_failures")),
        ("Phase-space failures", summary.get("phase_space_failures")),
        (
            "Maximum normalized energy-sum residual",
            summary.get("maximum_normalized_energy_sum_error"),
        ),
        (
            "Maximum direction norm error",
            summary.get("maximum_direction_norm_error"),
        ),
        (
            "Maximum opening-angle cosine error",
            summary.get("maximum_opening_angle_cosine_error"),
        ),
        (
            "99th-percentile opening-angle cosine error",
            summary.get("opening_angle_cosine_error_p99"),
        ),
        (
            "99.9th-percentile opening-angle cosine error",
            summary.get("opening_angle_cosine_error_p999"),
        ),
        (
            "Opening-angle pairs tested",
            summary.get("opening_angle_pairs_tested"),
        ),
        (
            "Opening-angle pairs skipped for soft photons",
            summary.get("opening_angle_pairs_skipped_soft"),
        ),
        ("Chi-square", summary.get("chi_square")),
        ("Degrees of freedom", summary.get("degrees_of_freedom")),
        ("Chi-square z score", summary.get("chi_square_z_score")),
        (
            "Maximum standardized residual",
            summary.get("maximum_standardized_residual"),
        ),
        (
            "Maximum marginal CDF difference",
            summary.get("maximum_marginal_cdf_difference"),
        ),
        (
            "Marginal CDF tolerance",
            summary.get("marginal_cdf_tolerance"),
        ),
        (
            "Grid bins per axis",
            summary.get("grid_bins_per_axis"),
        ),
        (
            "Integration subdivisions per axis",
            summary.get("integration_subdivisions_per_axis"),
        ),
    ]

    with output_path.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["metric", "value"])

        for metric, value in metrics:
            writer.writerow(
                [
                    metric,
                    "" if value is None else value,
                ]
            )


def validate_outputs(
    output_dir: Path,
    event_count: int,
) -> None:
    required_outputs = [
        "phase_space_long.csv",
        "observed_density_matrix.csv",
        "expected_density_matrix.csv",
        "residual_matrix.csv",
        "single_photon_spectrum.csv",
        "ordered_energy_spectra.csv",
        "opening_angle_spectra.csv",
        "validation_summary.csv",
    ]

    for filename in required_outputs:
        path = output_dir / filename

        if not path.is_file() or path.stat().st_size == 0:
            raise RuntimeError(f"Output was not created correctly: {path}")

    single_path = output_dir / "single_photon_spectrum.csv"

    with single_path.open("r", newline="") as handle:
        rows = list(csv.DictReader(handle))

    observed_photons = sum(
        int(row["observed_count"])
        for row in rows
    )

    expected_photons = 3 * event_count

    if observed_photons != expected_photons:
        raise RuntimeError(
            "Single-photon histogram total mismatch: "
            f"observed {observed_photons}, expected {expected_photons}"
        )

    ordered_path = output_dir / "ordered_energy_spectra.csv"

    with ordered_path.open("r", newline="") as handle:
        rows = list(csv.DictReader(handle))

    for column in ("low_count", "middle_count", "high_count"):
        total = sum(int(row[column]) for row in rows)

        if total != event_count:
            raise RuntimeError(
                f"{column} total mismatch: {total} != {event_count}"
            )

    angle_path = output_dir / "opening_angle_spectra.csv"

    with angle_path.open("r", newline="") as handle:
        rows = list(csv.DictReader(handle))

    all_pair_total = sum(
        int(row["all_pairs_count"])
        for row in rows
    )

    if all_pair_total != 3 * event_count:
        raise RuntimeError(
            "Opening-angle pair total mismatch: "
            f"{all_pair_total} != {3 * event_count}"
        )


def main() -> int:
    args = parse_args()

    if args.energy_bins < 5:
        raise ValueError("--energy-bins must be at least 5")

    if args.angle_bins < 5:
        raise ValueError("--angle-bins must be at least 5")

    require_file(args.gammas)
    require_file(args.grid)
    require_file(args.summary)

    args.output_dir.mkdir(parents=True, exist_ok=True)

    with args.summary.open("r") as handle:
        summary = json.load(handle)

    grid_rows = read_grid(args.grid)
    events = read_gamma_events(args.gammas)
    validate_three_gamma_events(events)

    event_count = len(events)

    expected_event_count = summary.get("events_analyzed")

    if (
        expected_event_count is not None
        and int(expected_event_count) != event_count
    ):
        raise ValueError(
            "Summary event count does not match gamma CSV: "
            f"{expected_event_count} != {event_count}"
        )

    phase_space_long_path = (
        args.output_dir / "phase_space_long.csv"
    )

    write_phase_space_long(
        grid_rows,
        event_count,
        phase_space_long_path,
    )

    for value_name, filename in (
        ("observed_density", "observed_density_matrix.csv"),
        ("expected_density", "expected_density_matrix.csv"),
        ("standardized_residual", "residual_matrix.csv"),
    ):
        r1_centers, r2_centers, matrix = matrix_from_grid(
            grid_rows,
            value_name,
            event_count,
        )

        write_matrix_csv(
            args.output_dir / filename,
            r1_centers,
            r2_centers,
            matrix,
        )

    write_single_photon_spectrum(
        events,
        grid_rows,
        args.output_dir / "single_photon_spectrum.csv",
        args.energy_bins,
    )

    write_ordered_energy_spectra(
        events,
        args.output_dir / "ordered_energy_spectra.csv",
        args.energy_bins,
    )

    write_opening_angle_spectra(
        events,
        args.output_dir / "opening_angle_spectra.csv",
        args.angle_bins,
    )

    write_validation_summary(
        summary,
        args.output_dir / "validation_summary.csv",
    )

    validate_outputs(args.output_dir, event_count)

    print("=== Ore-Powell figure-data export ===")
    print(f"Events analyzed : {event_count}")
    print(f"Gamma rows      : {3 * event_count}")
    print(f"Output directory: {args.output_dir}")
    print()
    print("Created:")

    for path in sorted(args.output_dir.glob("*.csv")):
        print(f"  {path.name}")

    print()
    print("PASS")

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
