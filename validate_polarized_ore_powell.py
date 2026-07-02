#!/usr/bin/env python3
"""
Scientific characterization and validation of polarized Ore-Powell output.

The validator tests:
- three photons per event;
- valid polarization truth for every photon;
- direction and polarization vector norms;
- polarization perpendicular to photon momentum;
- polarization orientation relative to the decay plane;
- polarization orientation relative to the global z quantization axis;
- pairwise polarization correlations;
- polarization correlations by ordered photon energy;
- optional comparison of polarized and ordinary Ore-Powell energy spectra.

Important limitation
--------------------
The current truth output does not record the sampled positronium magnetic
substate m = -1, 0, +1. This script therefore characterizes the ensemble
produced by Geant4 but cannot directly validate substate-specific behavior.

Outputs:
    <prefix>_summary.json
    <prefix>_metrics.csv
    <prefix>_histograms.csv
    <prefix>_event_correlations.csv

Example:
    python validate_polarized_ore_powell.py \
        polarized_run/annihilation_gammas.csv \
        --ordinary-csv ordinary_run/annihilation_gammas.csv \
        --output-prefix polarized_run/polarized_validation
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any

import numpy as np


ELECTRON_MASS_MEV = 0.51099895

REQUIRED_COLUMNS = {
    "event_id",
    "kinetic_energy_MeV",
    "dir_x",
    "dir_y",
    "dir_z",
    "pol_x",
    "pol_y",
    "pol_z",
    "polarization_valid",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate polarized Ore-Powell photon correlations."
    )

    parser.add_argument(
        "polarized_csv",
        type=Path,
        help="Polarized annihilation_gammas.csv.",
    )

    parser.add_argument(
        "--ordinary-csv",
        type=Path,
        default=None,
        help=(
            "Optional ordinary Ore-Powell annihilation_gammas.csv "
            "for energy-spectrum comparison."
        ),
    )

    parser.add_argument(
        "--output-prefix",
        type=Path,
        default=Path("polarized_ore_powell"),
        help="Output filename prefix.",
    )

    parser.add_argument(
        "--histogram-bins",
        type=int,
        default=50,
        help="Number of bins for correlation histograms.",
    )

    parser.add_argument(
        "--energy-bins",
        type=int,
        default=100,
        help="Number of bins for energy-spectrum comparison.",
    )

    parser.add_argument(
        "--norm-tolerance",
        type=float,
        default=2.0e-5,
        help="Maximum vector-norm deviation from unity.",
    )

    parser.add_argument(
        "--orthogonality-tolerance",
        type=float,
        default=2.0e-5,
        help="Maximum |momentum direction dot polarization|.",
    )

    parser.add_argument(
        "--energy-cdf-tolerance",
        type=float,
        default=0.02,
        help=(
            "Maximum two-sample binned CDF difference between ordinary "
            "and polarized photon-energy spectra."
        ),
    )

    return parser.parse_args()


def normalize(vector: np.ndarray) -> tuple[np.ndarray, float]:
    norm = float(np.linalg.norm(vector))

    if not math.isfinite(norm) or norm <= 0.0:
        raise ValueError(f"Cannot normalize vector with norm {norm}")

    return vector / norm, norm


def read_photon_events(
    path: Path,
    require_polarization: bool,
) -> dict[int, list[dict[str, Any]]]:
    if not path.is_file():
        raise FileNotFoundError(f"Input file not found: {path}")

    events: dict[int, list[dict[str, Any]]] = defaultdict(list)

    with path.open("r", newline="") as handle:
        reader = csv.DictReader(handle)
        columns = set(reader.fieldnames or [])

        required = {
            "event_id",
            "kinetic_energy_MeV",
            "dir_x",
            "dir_y",
            "dir_z",
        }

        if require_polarization:
            required |= {
                "pol_x",
                "pol_y",
                "pol_z",
                "polarization_valid",
            }

        missing = required - columns

        if missing:
            raise ValueError(
                f"{path}: missing required columns: {sorted(missing)}"
            )

        for line_number, row in enumerate(reader, start=2):
            direction_raw = np.asarray(
                [
                    float(row["dir_x"]),
                    float(row["dir_y"]),
                    float(row["dir_z"]),
                ],
                dtype=float,
            )

            direction, direction_norm = normalize(direction_raw)

            photon: dict[str, Any] = {
                "energy_MeV": float(row["kinetic_energy_MeV"]),
                "direction": direction,
                "direction_norm": direction_norm,
            }

            if require_polarization:
                polarization_raw = np.asarray(
                    [
                        float(row["pol_x"]),
                        float(row["pol_y"]),
                        float(row["pol_z"]),
                    ],
                    dtype=float,
                )

                polarization_norm = float(
                    np.linalg.norm(polarization_raw)
                )

                photon["polarization_raw"] = polarization_raw
                photon["polarization_norm"] = polarization_norm
                photon["polarization_valid"] = (
                    int(row["polarization_valid"]) != 0
                )
                photon["line_number"] = line_number

                if polarization_norm > 0.0:
                    photon["polarization"] = (
                        polarization_raw / polarization_norm
                    )
                else:
                    photon["polarization"] = polarization_raw

            event_id = int(row["event_id"])
            events[event_id].append(photon)

    if not events:
        raise ValueError(f"{path}: no photon events found")

    return dict(events)


def empirical_cdf_difference(
    values_a: list[float],
    values_b: list[float],
    bins: int,
    low: float,
    high: float,
) -> float:
    edges = np.linspace(low, high, bins + 1)

    counts_a, _ = np.histogram(values_a, bins=edges)
    counts_b, _ = np.histogram(values_b, bins=edges)

    if counts_a.sum() == 0 or counts_b.sum() == 0:
        return math.nan

    cdf_a = np.cumsum(counts_a) / counts_a.sum()
    cdf_b = np.cumsum(counts_b) / counts_b.sum()

    return float(np.max(np.abs(cdf_a - cdf_b)))


def angle_degrees_from_dot(dot_value: float) -> float:
    dot_value = max(-1.0, min(1.0, dot_value))
    return math.degrees(math.acos(dot_value))


def percentile(values: list[float], value: float) -> float:
    if not values:
        return math.nan

    return float(np.percentile(np.asarray(values), value))


def add_histogram(
    rows: list[dict[str, Any]],
    name: str,
    values: list[float],
    low: float,
    high: float,
    bins: int,
) -> None:
    counts, edges = np.histogram(
        np.asarray(values, dtype=float),
        bins=bins,
        range=(low, high),
    )

    total = int(counts.sum())

    for index, count in enumerate(counts):
        width = float(edges[index + 1] - edges[index])
        probability = float(count / total) if total else 0.0

        rows.append(
            {
                "distribution": name,
                "bin_low": float(edges[index]),
                "bin_high": float(edges[index + 1]),
                "bin_center": float(
                    0.5 * (edges[index] + edges[index + 1])
                ),
                "count": int(count),
                "probability": probability,
                "density": (
                    probability / width
                    if width > 0.0
                    else math.nan
                ),
            }
        )


def summarize_values(values: list[float]) -> dict[str, float]:
    array = np.asarray(values, dtype=float)

    return {
        "count": int(array.size),
        "mean": float(np.mean(array)),
        "standard_deviation": float(np.std(array)),
        "minimum": float(np.min(array)),
        "p01": float(np.percentile(array, 1.0)),
        "p50": float(np.percentile(array, 50.0)),
        "p99": float(np.percentile(array, 99.0)),
        "maximum": float(np.max(array)),
    }


def main() -> int:
    args = parse_args()

    if args.histogram_bins < 5:
        raise ValueError("--histogram-bins must be at least 5")

    if args.energy_bins < 5:
        raise ValueError("--energy-bins must be at least 5")

    events = read_photon_events(
        args.polarized_csv,
        require_polarization=True,
    )

    failures: list[str] = []

    multiplicity_failures = 0
    invalid_polarization_count = 0
    plane_normal_failures = 0

    direction_norm_errors: list[float] = []
    polarization_norm_errors: list[float] = []
    momentum_polarization_dots: list[float] = []

    polarization_plane_normal_abs_dots: list[float] = []
    polarization_in_plane_magnitudes: list[float] = []
    polarization_z_abs_dots: list[float] = []

    low_plane_normal_abs_dots: list[float] = []
    middle_plane_normal_abs_dots: list[float] = []
    high_plane_normal_abs_dots: list[float] = []

    low_z_abs_dots: list[float] = []
    middle_z_abs_dots: list[float] = []
    high_z_abs_dots: list[float] = []

    polarization_pair_dots: list[float] = []
    polarization_pair_abs_dots: list[float] = []
    low_middle_pol_dots: list[float] = []
    low_high_pol_dots: list[float] = []
    middle_high_pol_dots: list[float] = []

    polarization_angle_to_plane_normal_deg: list[float] = []
    polarization_angle_to_z_deg: list[float] = []

    event_rows: list[dict[str, Any]] = []
    polarized_energies_x: list[float] = []

    z_axis = np.asarray([0.0, 0.0, 1.0], dtype=float)

    valid_events = 0

    for event_id, photons in sorted(events.items()):
        if len(photons) != 3:
            multiplicity_failures += 1
            continue

        ordered = sorted(
            photons,
            key=lambda photon: photon["energy_MeV"],
        )

        directions = [
            photon["direction"]
            for photon in ordered
        ]

        polarizations: list[np.ndarray] = []
        event_valid = True

        for photon in ordered:
            polarized_energies_x.append(
                photon["energy_MeV"] / ELECTRON_MASS_MEV
            )

            direction_norm_errors.append(
                abs(photon["direction_norm"] - 1.0)
            )

            if not photon["polarization_valid"]:
                invalid_polarization_count += 1
                event_valid = False
                continue

            polarization_norm = photon["polarization_norm"]

            if (
                not math.isfinite(polarization_norm)
                or polarization_norm <= 0.0
            ):
                invalid_polarization_count += 1
                event_valid = False
                continue

            polarization_norm_errors.append(
                abs(polarization_norm - 1.0)
            )

            polarization = photon["polarization"]
            polarizations.append(polarization)

            momentum_polarization_dots.append(
                float(np.dot(photon["direction"], polarization))
            )

        if not event_valid or len(polarizations) != 3:
            continue

        plane_normal_raw = np.cross(
            directions[0],
            directions[1],
        )

        plane_norm = float(np.linalg.norm(plane_normal_raw))

        if plane_norm <= 1.0e-14:
            plane_normal_failures += 1
            continue

        plane_normal = plane_normal_raw / plane_norm

        # Plane normals have no physical sign. Use absolute projections.
        event_plane_abs: list[float] = []
        event_z_abs: list[float] = []

        for polarization in polarizations:
            plane_abs_dot = abs(
                float(np.dot(polarization, plane_normal))
            )

            z_abs_dot = abs(
                float(np.dot(polarization, z_axis))
            )

            in_plane_magnitude = math.sqrt(
                max(0.0, 1.0 - plane_abs_dot**2)
            )

            polarization_plane_normal_abs_dots.append(
                plane_abs_dot
            )

            polarization_in_plane_magnitudes.append(
                in_plane_magnitude
            )

            polarization_z_abs_dots.append(z_abs_dot)

            polarization_angle_to_plane_normal_deg.append(
                math.degrees(math.acos(
                    max(-1.0, min(1.0, plane_abs_dot))
                ))
            )

            polarization_angle_to_z_deg.append(
                math.degrees(math.acos(
                    max(-1.0, min(1.0, z_abs_dot))
                ))
            )

            event_plane_abs.append(plane_abs_dot)
            event_z_abs.append(z_abs_dot)

        low_plane_normal_abs_dots.append(event_plane_abs[0])
        middle_plane_normal_abs_dots.append(event_plane_abs[1])
        high_plane_normal_abs_dots.append(event_plane_abs[2])

        low_z_abs_dots.append(event_z_abs[0])
        middle_z_abs_dots.append(event_z_abs[1])
        high_z_abs_dots.append(event_z_abs[2])

        dot_lm = float(np.dot(polarizations[0], polarizations[1]))
        dot_lh = float(np.dot(polarizations[0], polarizations[2]))
        dot_mh = float(np.dot(polarizations[1], polarizations[2]))

        low_middle_pol_dots.append(dot_lm)
        low_high_pol_dots.append(dot_lh)
        middle_high_pol_dots.append(dot_mh)

        polarization_pair_dots.extend(
            [dot_lm, dot_lh, dot_mh]
        )

        polarization_pair_abs_dots.extend(
            [abs(dot_lm), abs(dot_lh), abs(dot_mh)]
        )

        event_rows.append(
            {
                "event_id": event_id,
                "low_energy_x": (
                    ordered[0]["energy_MeV"] / ELECTRON_MASS_MEV
                ),
                "middle_energy_x": (
                    ordered[1]["energy_MeV"] / ELECTRON_MASS_MEV
                ),
                "high_energy_x": (
                    ordered[2]["energy_MeV"] / ELECTRON_MASS_MEV
                ),
                "low_pol_plane_normal_abs_dot": event_plane_abs[0],
                "middle_pol_plane_normal_abs_dot": event_plane_abs[1],
                "high_pol_plane_normal_abs_dot": event_plane_abs[2],
                "low_pol_z_abs_dot": event_z_abs[0],
                "middle_pol_z_abs_dot": event_z_abs[1],
                "high_pol_z_abs_dot": event_z_abs[2],
                "low_middle_pol_dot": dot_lm,
                "low_high_pol_dot": dot_lh,
                "middle_high_pol_dot": dot_mh,
            }
        )

        valid_events += 1

    photon_count = sum(len(photons) for photons in events.values())

    maximum_direction_norm_error = max(
        direction_norm_errors,
        default=math.nan,
    )

    maximum_polarization_norm_error = max(
        polarization_norm_errors,
        default=math.nan,
    )

    maximum_abs_momentum_polarization_dot = max(
        (abs(value) for value in momentum_polarization_dots),
        default=math.nan,
    )

    if multiplicity_failures:
        failures.append(
            f"Multiplicity failures: {multiplicity_failures}"
        )

    if invalid_polarization_count:
        failures.append(
            f"Invalid polarization photons: {invalid_polarization_count}"
        )

    if plane_normal_failures:
        failures.append(
            f"Plane-normal failures: {plane_normal_failures}"
        )

    if (
        not math.isfinite(maximum_direction_norm_error)
        or maximum_direction_norm_error > args.norm_tolerance
    ):
        failures.append(
            "Maximum direction norm error "
            f"{maximum_direction_norm_error:.6g} exceeds "
            f"{args.norm_tolerance:.6g}"
        )

    if (
        not math.isfinite(maximum_polarization_norm_error)
        or maximum_polarization_norm_error > args.norm_tolerance
    ):
        failures.append(
            "Maximum polarization norm error "
            f"{maximum_polarization_norm_error:.6g} exceeds "
            f"{args.norm_tolerance:.6g}"
        )

    if (
        not math.isfinite(maximum_abs_momentum_polarization_dot)
        or maximum_abs_momentum_polarization_dot
        > args.orthogonality_tolerance
    ):
        failures.append(
            "Maximum |direction dot polarization| "
            f"{maximum_abs_momentum_polarization_dot:.6g} exceeds "
            f"{args.orthogonality_tolerance:.6g}"
        )

    ordinary_comparison: dict[str, Any] | None = None

    if args.ordinary_csv is not None:
        ordinary_events = read_photon_events(
            args.ordinary_csv,
            require_polarization=False,
        )

        ordinary_energies_x = [
            photon["energy_MeV"] / ELECTRON_MASS_MEV
            for photons in ordinary_events.values()
            for photon in photons
        ]

        cdf_difference = empirical_cdf_difference(
            polarized_energies_x,
            ordinary_energies_x,
            bins=args.energy_bins,
            low=0.0,
            high=1.0,
        )

        ordinary_comparison = {
            "ordinary_input_csv": str(args.ordinary_csv),
            "ordinary_event_count": len(ordinary_events),
            "ordinary_photon_count": len(ordinary_energies_x),
            "polarized_photon_count": len(polarized_energies_x),
            "maximum_binned_energy_cdf_difference": cdf_difference,
            "energy_cdf_tolerance": args.energy_cdf_tolerance,
        }

        if (
            not math.isfinite(cdf_difference)
            or cdf_difference > args.energy_cdf_tolerance
        ):
            failures.append(
                "Ordinary/polarized energy CDF difference "
                f"{cdf_difference:.6g} exceeds "
                f"{args.energy_cdf_tolerance:.6g}"
            )

    status = "PASS" if not failures else "FAIL"

    summary = {
        "status": status,
        "input_csv": str(args.polarized_csv),
        "events_read": len(events),
        "events_analyzed": valid_events,
        "photon_count": photon_count,
        "multiplicity_failures": multiplicity_failures,
        "invalid_polarization_count": invalid_polarization_count,
        "plane_normal_failures": plane_normal_failures,
        "truth_limitations": [
            (
                "Magnetic substate m=-1,0,+1 is not recorded in the "
                "current event truth output."
            ),
            (
                "Substate-specific validation cannot be performed from "
                "annihilation_gammas.csv."
            ),
        ],
        "fundamental_vector_checks": {
            "maximum_direction_norm_error": (
                maximum_direction_norm_error
            ),
            "maximum_polarization_norm_error": (
                maximum_polarization_norm_error
            ),
            "maximum_abs_direction_dot_polarization": (
                maximum_abs_momentum_polarization_dot
            ),
            "norm_tolerance": args.norm_tolerance,
            "orthogonality_tolerance": (
                args.orthogonality_tolerance
            ),
        },
        "polarization_relative_to_decay_plane": {
            "absolute_dot_with_plane_normal": summarize_values(
                polarization_plane_normal_abs_dots
            ),
            "in_plane_component_magnitude": summarize_values(
                polarization_in_plane_magnitudes
            ),
            "angle_to_plane_normal_degrees": summarize_values(
                polarization_angle_to_plane_normal_deg
            ),
            "by_ordered_energy": {
                "low": summarize_values(
                    low_plane_normal_abs_dots
                ),
                "middle": summarize_values(
                    middle_plane_normal_abs_dots
                ),
                "high": summarize_values(
                    high_plane_normal_abs_dots
                ),
            },
        },
        "polarization_relative_to_global_z_axis": {
            "absolute_dot_with_z": summarize_values(
                polarization_z_abs_dots
            ),
            "angle_to_z_degrees": summarize_values(
                polarization_angle_to_z_deg
            ),
            "by_ordered_energy": {
                "low": summarize_values(low_z_abs_dots),
                "middle": summarize_values(middle_z_abs_dots),
                "high": summarize_values(high_z_abs_dots),
            },
        },
        "pairwise_polarization_correlations": {
            "all_signed_dots": summarize_values(
                polarization_pair_dots
            ),
            "all_absolute_dots": summarize_values(
                polarization_pair_abs_dots
            ),
            "low_middle_signed_dot": summarize_values(
                low_middle_pol_dots
            ),
            "low_high_signed_dot": summarize_values(
                low_high_pol_dots
            ),
            "middle_high_signed_dot": summarize_values(
                middle_high_pol_dots
            ),
        },
        "ordinary_backend_comparison": ordinary_comparison,
        "failures": failures,
    }

    output_prefix = args.output_prefix
    output_prefix.parent.mkdir(parents=True, exist_ok=True)

    summary_path = Path(f"{output_prefix}_summary.json")
    metrics_path = Path(f"{output_prefix}_metrics.csv")
    histograms_path = Path(f"{output_prefix}_histograms.csv")
    event_path = Path(f"{output_prefix}_event_correlations.csv")

    with summary_path.open("w") as handle:
        json.dump(summary, handle, indent=2, sort_keys=True)
        handle.write("\n")

    metric_rows: list[dict[str, Any]] = []

    def metric(
        category: str,
        name: str,
        value: Any,
        metric_status: str = "",
    ) -> None:
        metric_rows.append(
            {
                "category": category,
                "metric": name,
                "value": value,
                "status": metric_status,
            }
        )

    metric("run", "status", status, status)
    metric("run", "events_read", len(events))
    metric("run", "events_analyzed", valid_events)
    metric("run", "photon_count", photon_count)

    metric(
        "fundamental",
        "multiplicity_failures",
        multiplicity_failures,
        "PASS" if multiplicity_failures == 0 else "FAIL",
    )

    metric(
        "fundamental",
        "invalid_polarization_count",
        invalid_polarization_count,
        "PASS" if invalid_polarization_count == 0 else "FAIL",
    )

    metric(
        "fundamental",
        "maximum_direction_norm_error",
        maximum_direction_norm_error,
        (
            "PASS"
            if maximum_direction_norm_error <= args.norm_tolerance
            else "FAIL"
        ),
    )

    metric(
        "fundamental",
        "maximum_polarization_norm_error",
        maximum_polarization_norm_error,
        (
            "PASS"
            if maximum_polarization_norm_error
            <= args.norm_tolerance
            else "FAIL"
        ),
    )

    metric(
        "fundamental",
        "maximum_abs_direction_dot_polarization",
        maximum_abs_momentum_polarization_dot,
        (
            "PASS"
            if maximum_abs_momentum_polarization_dot
            <= args.orthogonality_tolerance
            else "FAIL"
        ),
    )

    correlation_groups = {
        "plane_normal_abs_dot": (
            polarization_plane_normal_abs_dots
        ),
        "in_plane_component": (
            polarization_in_plane_magnitudes
        ),
        "global_z_abs_dot": polarization_z_abs_dots,
        "polarization_pair_signed_dot": polarization_pair_dots,
        "polarization_pair_abs_dot": (
            polarization_pair_abs_dots
        ),
        "low_middle_pol_dot": low_middle_pol_dots,
        "low_high_pol_dot": low_high_pol_dots,
        "middle_high_pol_dot": middle_high_pol_dots,
    }

    for name, values in correlation_groups.items():
        statistics = summarize_values(values)

        for statistic_name, value in statistics.items():
            metric(
                "correlation",
                f"{name}_{statistic_name}",
                value,
            )

    if ordinary_comparison is not None:
        cdf_difference = ordinary_comparison[
            "maximum_binned_energy_cdf_difference"
        ]

        metric(
            "ordinary_comparison",
            "maximum_binned_energy_cdf_difference",
            cdf_difference,
            (
                "PASS"
                if cdf_difference <= args.energy_cdf_tolerance
                else "FAIL"
            ),
        )

    with metrics_path.open("w", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "category",
                "metric",
                "value",
                "status",
            ],
        )

        writer.writeheader()
        writer.writerows(metric_rows)

    histogram_rows: list[dict[str, Any]] = []

    add_histogram(
        histogram_rows,
        "polarization_plane_normal_abs_dot",
        polarization_plane_normal_abs_dots,
        0.0,
        1.0,
        args.histogram_bins,
    )

    add_histogram(
        histogram_rows,
        "polarization_in_plane_component",
        polarization_in_plane_magnitudes,
        0.0,
        1.0,
        args.histogram_bins,
    )

    add_histogram(
        histogram_rows,
        "polarization_global_z_abs_dot",
        polarization_z_abs_dots,
        0.0,
        1.0,
        args.histogram_bins,
    )

    add_histogram(
        histogram_rows,
        "polarization_pair_signed_dot",
        polarization_pair_dots,
        -1.0,
        1.0,
        args.histogram_bins,
    )

    add_histogram(
        histogram_rows,
        "low_middle_polarization_dot",
        low_middle_pol_dots,
        -1.0,
        1.0,
        args.histogram_bins,
    )

    add_histogram(
        histogram_rows,
        "low_high_polarization_dot",
        low_high_pol_dots,
        -1.0,
        1.0,
        args.histogram_bins,
    )

    add_histogram(
        histogram_rows,
        "middle_high_polarization_dot",
        middle_high_pol_dots,
        -1.0,
        1.0,
        args.histogram_bins,
    )

    add_histogram(
        histogram_rows,
        "polarization_angle_to_plane_normal_deg",
        polarization_angle_to_plane_normal_deg,
        0.0,
        90.0,
        args.histogram_bins,
    )

    add_histogram(
        histogram_rows,
        "polarization_angle_to_global_z_deg",
        polarization_angle_to_z_deg,
        0.0,
        90.0,
        args.histogram_bins,
    )

    with histograms_path.open("w", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "distribution",
                "bin_low",
                "bin_high",
                "bin_center",
                "count",
                "probability",
                "density",
            ],
        )

        writer.writeheader()
        writer.writerows(histogram_rows)

    with event_path.open("w", newline="") as handle:
        fieldnames = [
            "event_id",
            "low_energy_x",
            "middle_energy_x",
            "high_energy_x",
            "low_pol_plane_normal_abs_dot",
            "middle_pol_plane_normal_abs_dot",
            "high_pol_plane_normal_abs_dot",
            "low_pol_z_abs_dot",
            "middle_pol_z_abs_dot",
            "high_pol_z_abs_dot",
            "low_middle_pol_dot",
            "low_high_pol_dot",
            "middle_high_pol_dot",
        ]

        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(event_rows)

    print("=== Polarized Ore-Powell Validation ===")
    print(f"Input CSV                        : {args.polarized_csv}")
    print(f"Events read                      : {len(events)}")
    print(f"Events analyzed                  : {valid_events}")
    print(f"Photon count                     : {photon_count}")
    print(f"Multiplicity failures            : {multiplicity_failures}")
    print(f"Invalid polarization photons     : {invalid_polarization_count}")
    print(f"Plane-normal failures            : {plane_normal_failures}")
    print()

    print("Fundamental vector checks")
    print(
        "  max direction norm error       : "
        f"{maximum_direction_norm_error:.6e}"
    )
    print(
        "  max polarization norm error    : "
        f"{maximum_polarization_norm_error:.6e}"
    )
    print(
        "  max |direction dot polarization|: "
        f"{maximum_abs_momentum_polarization_dot:.6e}"
    )
    print()

    print("Ensemble polarization correlations")
    print(
        "  mean |polarization dot plane normal|: "
        f"{np.mean(polarization_plane_normal_abs_dots):.6f}"
    )
    print(
        "  mean |polarization dot global z|    : "
        f"{np.mean(polarization_z_abs_dots):.6f}"
    )
    print(
        "  mean pairwise polarization dot      : "
        f"{np.mean(polarization_pair_dots):.6f}"
    )
    print(
        "  mean |pairwise polarization dot|    : "
        f"{np.mean(polarization_pair_abs_dots):.6f}"
    )

    if ordinary_comparison is not None:
        print()
        print("Ordinary/polarized energy comparison")
        print(
            "  maximum binned CDF difference       : "
            f"{ordinary_comparison['maximum_binned_energy_cdf_difference']:.6e}"
        )
        print(
            "  tolerance                           : "
            f"{args.energy_cdf_tolerance:.6e}"
        )

    print()
    print(f"Summary JSON                     : {summary_path}")
    print(f"Metrics CSV                      : {metrics_path}")
    print(f"Histograms CSV                   : {histograms_path}")
    print(f"Event correlations CSV           : {event_path}")
    print()
    print(status)

    if failures:
        print()
        print("Failures:")

        for failure in failures:
            print(f"  - {failure}")

        return 1

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
