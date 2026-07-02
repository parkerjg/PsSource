#!/usr/bin/env python3
"""
Validate three-gamma event geometry.

Tests:
- photon-direction isotropy;
- decay-plane normal isotropy;
- azimuthal uniformity;
- polar-angle uniformity;
- Cartesian-axis alignment;
- ordered pair opening-angle distributions;
- event coplanarity;
- energy and momentum closure.

Outputs:
- <prefix>_summary.json
- <prefix>_metrics.csv
- <prefix>_histograms.csv

Example:
    python validate_three_gamma_geometry.py \
        ore_powell_benchmark_run/annihilation_gammas.csv \
        --output-prefix ore_powell_benchmark_run/three_gamma_geometry
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
TOTAL_ENERGY_MEV = 2.0 * ELECTRON_MASS_MEV

REQUIRED_COLUMNS = {
    "event_id",
    "kinetic_energy_MeV",
    "dir_x",
    "dir_y",
    "dir_z",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate three-gamma angular geometry."
    )

    parser.add_argument(
        "gammas",
        type=Path,
        help="Path to annihilation_gammas.csv.",
    )

    parser.add_argument(
        "--output-prefix",
        type=Path,
        default=Path("three_gamma_geometry"),
        help="Prefix for JSON and CSV outputs.",
    )

    parser.add_argument(
        "--cos-bins",
        type=int,
        default=20,
        help="Number of bins for cos(theta).",
    )

    parser.add_argument(
        "--phi-bins",
        type=int,
        default=24,
        help="Number of bins for azimuth.",
    )

    parser.add_argument(
        "--angle-bins",
        type=int,
        default=90,
        help="Number of bins for opening angles from 0 to 180 degrees.",
    )

    parser.add_argument(
        "--uniformity-z-limit",
        type=float,
        default=6.0,
        help="Maximum absolute chi-square z score for uniformity tests.",
    )

    parser.add_argument(
        "--axis-mean-tolerance",
        type=float,
        default=0.01,
        help="Tolerance for mean absolute Cartesian component from 0.5.",
    )

    parser.add_argument(
        "--axis-asymmetry-tolerance",
        type=float,
        default=0.01,
        help="Maximum difference among mean absolute x, y, and z components.",
    )

    parser.add_argument(
        "--coplanarity-tolerance",
        type=float,
        default=1.0e-6,
        help="Maximum allowed absolute scalar triple product.",
    )

    parser.add_argument(
        "--energy-sum-tolerance-mev",
        type=float,
        default=5.0e-6,
        help="Maximum allowed event energy-sum residual.",
    )

    parser.add_argument(
        "--momentum-tolerance-mev",
        type=float,
        default=5.0e-6,
        help="Maximum allowed event momentum-closure residual.",
    )

    return parser.parse_args()


def read_events(
    path: Path,
) -> dict[int, list[tuple[float, np.ndarray]]]:
    if not path.is_file():
        raise FileNotFoundError(f"Input file not found: {path}")

    events: dict[int, list[tuple[float, np.ndarray]]] = defaultdict(list)

    with path.open("r", newline="") as handle:
        reader = csv.DictReader(handle)
        columns = set(reader.fieldnames or [])
        missing = REQUIRED_COLUMNS - columns

        if missing:
            raise ValueError(
                f"{path}: missing required columns: {sorted(missing)}"
            )

        for line_number, row in enumerate(reader, start=2):
            event_id = int(row["event_id"])
            energy = float(row["kinetic_energy_MeV"])

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
                    f"{path}:{line_number}: invalid direction norm {norm}"
                )

            direction /= norm
            events[event_id].append((energy, direction))

    if not events:
        raise ValueError(f"{path}: no photon events found")

    return dict(events)


def chi_square_uniform(
    values: np.ndarray,
    low: float,
    high: float,
    bins: int,
) -> dict[str, Any]:
    counts, edges = np.histogram(
        values,
        bins=bins,
        range=(low, high),
    )

    total = int(counts.sum())
    expected = total / bins

    if total == 0 or expected <= 0.0:
        return {
            "counts": counts,
            "edges": edges,
            "chi_square": math.nan,
            "degrees_of_freedom": bins - 1,
            "z_score": math.nan,
        }

    chi_square = float(
        np.sum((counts - expected) ** 2 / expected)
    )

    degrees_of_freedom = bins - 1

    z_score = (
        chi_square - degrees_of_freedom
    ) / math.sqrt(2.0 * degrees_of_freedom)

    return {
        "counts": counts,
        "edges": edges,
        "chi_square": chi_square,
        "degrees_of_freedom": degrees_of_freedom,
        "z_score": z_score,
    }


def angle_degrees(a: np.ndarray, b: np.ndarray) -> float:
    cosine = float(np.dot(a, b))
    cosine = max(-1.0, min(1.0, cosine))
    return math.degrees(math.acos(cosine))


def percentile(values: list[float], q: float) -> float:
    if not values:
        return math.nan

    return float(np.percentile(np.asarray(values), q))


def add_metric(
    rows: list[dict[str, Any]],
    category: str,
    metric: str,
    value: Any,
    status: str = "",
) -> None:
    rows.append(
        {
            "category": category,
            "metric": metric,
            "value": value,
            "status": status,
        }
    )


def add_histogram_rows(
    output_rows: list[dict[str, Any]],
    distribution: str,
    counts: np.ndarray,
    edges: np.ndarray,
) -> None:
    total = int(counts.sum())

    for index, count in enumerate(counts):
        width = float(edges[index + 1] - edges[index])
        probability = float(count / total) if total > 0 else 0.0
        density = probability / width if width > 0.0 else math.nan

        output_rows.append(
            {
                "distribution": distribution,
                "bin_low": float(edges[index]),
                "bin_high": float(edges[index + 1]),
                "bin_center": float(
                    0.5 * (edges[index] + edges[index + 1])
                ),
                "count": int(count),
                "probability": probability,
                "density": density,
            }
        )


def main() -> int:
    args = parse_args()

    if args.cos_bins < 5:
        raise ValueError("--cos-bins must be at least 5")

    if args.phi_bins < 5:
        raise ValueError("--phi-bins must be at least 5")

    if args.angle_bins < 5:
        raise ValueError("--angle-bins must be at least 5")

    events = read_events(args.gammas)

    multiplicity_failures = 0

    photon_cos_theta: list[float] = []
    photon_phi: list[float] = []

    plane_cos_theta: list[float] = []
    plane_phi: list[float] = []

    abs_x: list[float] = []
    abs_y: list[float] = []
    abs_z: list[float] = []

    low_middle_angles: list[float] = []
    low_high_angles: list[float] = []
    middle_high_angles: list[float] = []
    all_opening_angles: list[float] = []

    coplanarity_values: list[float] = []
    energy_sum_errors: list[float] = []
    momentum_errors: list[float] = []
    plane_normal_failures = 0

    valid_event_count = 0

    for photons in events.values():
        if len(photons) != 3:
            multiplicity_failures += 1
            continue

        ordered = sorted(photons, key=lambda item: item[0])

        energies = np.asarray(
            [item[0] for item in ordered],
            dtype=float,
        )

        directions = [
            item[1]
            for item in ordered
        ]

        valid_event_count += 1

        for direction in directions:
            photon_cos_theta.append(float(direction[2]))

            phi = math.atan2(
                float(direction[1]),
                float(direction[0]),
            )

            if phi < 0.0:
                phi += 2.0 * math.pi

            photon_phi.append(phi)

            abs_x.append(abs(float(direction[0])))
            abs_y.append(abs(float(direction[1])))
            abs_z.append(abs(float(direction[2])))

        plane_normal = np.cross(
            directions[0],
            directions[1],
        )

        plane_norm = float(np.linalg.norm(plane_normal))

        if plane_norm <= 1.0e-14:
            plane_normal_failures += 1
        else:
            plane_normal /= plane_norm

            # Plane normals are equivalent under n -> -n.
            # Orient them into the positive-z hemisphere for stable output.
            if plane_normal[2] < 0.0:
                plane_normal *= -1.0

            plane_cos_theta.append(float(plane_normal[2]))

            phi = math.atan2(
                float(plane_normal[1]),
                float(plane_normal[0]),
            )

            if phi < 0.0:
                phi += 2.0 * math.pi

            plane_phi.append(phi)

        angle_lm = angle_degrees(
            directions[0],
            directions[1],
        )

        angle_lh = angle_degrees(
            directions[0],
            directions[2],
        )

        angle_mh = angle_degrees(
            directions[1],
            directions[2],
        )

        low_middle_angles.append(angle_lm)
        low_high_angles.append(angle_lh)
        middle_high_angles.append(angle_mh)

        all_opening_angles.extend(
            [angle_lm, angle_lh, angle_mh]
        )

        scalar_triple = abs(
            float(
                np.dot(
                    directions[0],
                    np.cross(
                        directions[1],
                        directions[2],
                    ),
                )
            )
        )

        coplanarity_values.append(scalar_triple)

        energy_sum = float(np.sum(energies))
        energy_sum_errors.append(
            abs(energy_sum - TOTAL_ENERGY_MEV)
        )

        momentum = np.zeros(3, dtype=float)

        for energy, direction in zip(energies, directions):
            momentum += energy * direction

        momentum_errors.append(
            float(np.linalg.norm(momentum))
        )

    if valid_event_count == 0:
        raise ValueError("No valid three-photon events were found")

    photon_cos_result = chi_square_uniform(
        np.asarray(photon_cos_theta),
        -1.0,
        1.0,
        args.cos_bins,
    )

    photon_phi_result = chi_square_uniform(
        np.asarray(photon_phi),
        0.0,
        2.0 * math.pi,
        args.phi_bins,
    )

    # Because plane normals were oriented into the positive-z hemisphere,
    # cos(theta) should be uniform on [0, 1].
    plane_cos_result = chi_square_uniform(
        np.asarray(plane_cos_theta),
        0.0,
        1.0,
        args.cos_bins,
    )

    plane_phi_result = chi_square_uniform(
        np.asarray(plane_phi),
        0.0,
        2.0 * math.pi,
        args.phi_bins,
    )

    mean_abs_x = float(np.mean(abs_x))
    mean_abs_y = float(np.mean(abs_y))
    mean_abs_z = float(np.mean(abs_z))

    axis_means = [mean_abs_x, mean_abs_y, mean_abs_z]
    axis_asymmetry = max(axis_means) - min(axis_means)

    max_coplanarity = max(coplanarity_values)
    max_energy_sum_error = max(energy_sum_errors)
    max_momentum_error = max(momentum_errors)

    failures: list[str] = []

    uniformity_results = {
        "photon_cos_theta": photon_cos_result,
        "photon_phi": photon_phi_result,
        "plane_normal_cos_theta": plane_cos_result,
        "plane_normal_phi": plane_phi_result,
    }

    for name, result in uniformity_results.items():
        z_score = float(result["z_score"])

        if (
            not math.isfinite(z_score)
            or abs(z_score) > args.uniformity_z_limit
        ):
            failures.append(
                f"{name} uniformity z score "
                f"{z_score:.6g} exceeds "
                f"{args.uniformity_z_limit:.6g}"
            )

    for name, value in (
        ("mean_abs_x", mean_abs_x),
        ("mean_abs_y", mean_abs_y),
        ("mean_abs_z", mean_abs_z),
    ):
        if abs(value - 0.5) > args.axis_mean_tolerance:
            failures.append(
                f"{name}={value:.6g} differs from 0.5 by more than "
                f"{args.axis_mean_tolerance:.6g}"
            )

    if axis_asymmetry > args.axis_asymmetry_tolerance:
        failures.append(
            f"Cartesian-axis asymmetry {axis_asymmetry:.6g} exceeds "
            f"{args.axis_asymmetry_tolerance:.6g}"
        )

    if max_coplanarity > args.coplanarity_tolerance:
        failures.append(
            f"Maximum coplanarity residual {max_coplanarity:.6g} exceeds "
            f"{args.coplanarity_tolerance:.6g}"
        )

    if max_energy_sum_error > args.energy_sum_tolerance_mev:
        failures.append(
            f"Maximum energy-sum error {max_energy_sum_error:.6g} MeV "
            f"exceeds {args.energy_sum_tolerance_mev:.6g} MeV"
        )

    if max_momentum_error > args.momentum_tolerance_mev:
        failures.append(
            f"Maximum momentum error {max_momentum_error:.6g} MeV/c "
            f"exceeds {args.momentum_tolerance_mev:.6g} MeV/c"
        )

    if multiplicity_failures != 0:
        failures.append(
            f"Multiplicity failures were nonzero: "
            f"{multiplicity_failures}"
        )

    if plane_normal_failures != 0:
        failures.append(
            f"Plane-normal failures were nonzero: "
            f"{plane_normal_failures}"
        )

    status = "PASS" if not failures else "FAIL"

    summary = {
        "status": status,
        "input_csv": str(args.gammas),
        "events_read": len(events),
        "events_analyzed": valid_event_count,
        "multiplicity_failures": multiplicity_failures,
        "plane_normal_failures": plane_normal_failures,
        "uniformity_z_limit": args.uniformity_z_limit,
        "axis_mean_tolerance": args.axis_mean_tolerance,
        "axis_asymmetry_tolerance": args.axis_asymmetry_tolerance,
        "coplanarity_tolerance": args.coplanarity_tolerance,
        "energy_sum_tolerance_MeV": args.energy_sum_tolerance_mev,
        "momentum_tolerance_MeV_over_c": (
            args.momentum_tolerance_mev
        ),
        "photon_cos_theta": {
            "chi_square": photon_cos_result["chi_square"],
            "degrees_of_freedom": (
                photon_cos_result["degrees_of_freedom"]
            ),
            "z_score": photon_cos_result["z_score"],
        },
        "photon_phi": {
            "chi_square": photon_phi_result["chi_square"],
            "degrees_of_freedom": (
                photon_phi_result["degrees_of_freedom"]
            ),
            "z_score": photon_phi_result["z_score"],
        },
        "plane_normal_cos_theta": {
            "chi_square": plane_cos_result["chi_square"],
            "degrees_of_freedom": (
                plane_cos_result["degrees_of_freedom"]
            ),
            "z_score": plane_cos_result["z_score"],
        },
        "plane_normal_phi": {
            "chi_square": plane_phi_result["chi_square"],
            "degrees_of_freedom": (
                plane_phi_result["degrees_of_freedom"]
            ),
            "z_score": plane_phi_result["z_score"],
        },
        "cartesian_axis_statistics": {
            "mean_abs_x": mean_abs_x,
            "mean_abs_y": mean_abs_y,
            "mean_abs_z": mean_abs_z,
            "maximum_mean_difference": axis_asymmetry,
        },
        "coplanarity": {
            "mean_absolute_scalar_triple_product": float(
                np.mean(coplanarity_values)
            ),
            "p99_absolute_scalar_triple_product": percentile(
                coplanarity_values,
                99.0,
            ),
            "p999_absolute_scalar_triple_product": percentile(
                coplanarity_values,
                99.9,
            ),
            "maximum_absolute_scalar_triple_product": (
                max_coplanarity
            ),
        },
        "energy_closure": {
            "mean_absolute_error_MeV": float(
                np.mean(energy_sum_errors)
            ),
            "p99_absolute_error_MeV": percentile(
                energy_sum_errors,
                99.0,
            ),
            "maximum_absolute_error_MeV": (
                max_energy_sum_error
            ),
        },
        "momentum_closure": {
            "mean_error_MeV_over_c": float(
                np.mean(momentum_errors)
            ),
            "p99_error_MeV_over_c": percentile(
                momentum_errors,
                99.0,
            ),
            "maximum_error_MeV_over_c": (
                max_momentum_error
            ),
        },
        "opening_angles_degrees": {
            "low_middle": {
                "mean": float(np.mean(low_middle_angles)),
                "std": float(np.std(low_middle_angles)),
                "minimum": min(low_middle_angles),
                "maximum": max(low_middle_angles),
            },
            "low_high": {
                "mean": float(np.mean(low_high_angles)),
                "std": float(np.std(low_high_angles)),
                "minimum": min(low_high_angles),
                "maximum": max(low_high_angles),
            },
            "middle_high": {
                "mean": float(np.mean(middle_high_angles)),
                "std": float(np.std(middle_high_angles)),
                "minimum": min(middle_high_angles),
                "maximum": max(middle_high_angles),
            },
        },
        "failures": failures,
    }

    output_prefix = args.output_prefix
    output_prefix.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    summary_path = Path(
        f"{output_prefix}_summary.json"
    )

    metrics_path = Path(
        f"{output_prefix}_metrics.csv"
    )

    histograms_path = Path(
        f"{output_prefix}_histograms.csv"
    )

    with summary_path.open("w") as handle:
        json.dump(
            summary,
            handle,
            indent=2,
            sort_keys=True,
        )
        handle.write("\n")

    metric_rows: list[dict[str, Any]] = []

    add_metric(
        metric_rows,
        "run",
        "status",
        status,
        status,
    )

    add_metric(
        metric_rows,
        "run",
        "events_read",
        len(events),
    )

    add_metric(
        metric_rows,
        "run",
        "events_analyzed",
        valid_event_count,
    )

    add_metric(
        metric_rows,
        "run",
        "multiplicity_failures",
        multiplicity_failures,
        "PASS" if multiplicity_failures == 0 else "FAIL",
    )

    add_metric(
        metric_rows,
        "run",
        "plane_normal_failures",
        plane_normal_failures,
        "PASS" if plane_normal_failures == 0 else "FAIL",
    )

    for name, result in uniformity_results.items():
        z_score = float(result["z_score"])
        test_status = (
            "PASS"
            if math.isfinite(z_score)
            and abs(z_score) <= args.uniformity_z_limit
            else "FAIL"
        )

        add_metric(
            metric_rows,
            name,
            "chi_square",
            result["chi_square"],
        )

        add_metric(
            metric_rows,
            name,
            "degrees_of_freedom",
            result["degrees_of_freedom"],
        )

        add_metric(
            metric_rows,
            name,
            "chi_square_z_score",
            z_score,
            test_status,
        )

    for name, value in (
        ("mean_abs_x", mean_abs_x),
        ("mean_abs_y", mean_abs_y),
        ("mean_abs_z", mean_abs_z),
    ):
        test_status = (
            "PASS"
            if abs(value - 0.5) <= args.axis_mean_tolerance
            else "FAIL"
        )

        add_metric(
            metric_rows,
            "cartesian_axis",
            name,
            value,
            test_status,
        )

    add_metric(
        metric_rows,
        "cartesian_axis",
        "maximum_mean_difference",
        axis_asymmetry,
        (
            "PASS"
            if axis_asymmetry <= args.axis_asymmetry_tolerance
            else "FAIL"
        ),
    )

    add_metric(
        metric_rows,
        "coplanarity",
        "mean_absolute_scalar_triple_product",
        float(np.mean(coplanarity_values)),
    )

    add_metric(
        metric_rows,
        "coplanarity",
        "p99_absolute_scalar_triple_product",
        percentile(coplanarity_values, 99.0),
    )

    add_metric(
        metric_rows,
        "coplanarity",
        "p999_absolute_scalar_triple_product",
        percentile(coplanarity_values, 99.9),
    )

    add_metric(
        metric_rows,
        "coplanarity",
        "maximum_absolute_scalar_triple_product",
        max_coplanarity,
        (
            "PASS"
            if max_coplanarity <= args.coplanarity_tolerance
            else "FAIL"
        ),
    )

    add_metric(
        metric_rows,
        "energy_closure",
        "maximum_absolute_error_MeV",
        max_energy_sum_error,
        (
            "PASS"
            if max_energy_sum_error <= args.energy_sum_tolerance_mev
            else "FAIL"
        ),
    )

    add_metric(
        metric_rows,
        "momentum_closure",
        "maximum_error_MeV_over_c",
        max_momentum_error,
        (
            "PASS"
            if max_momentum_error <= args.momentum_tolerance_mev
            else "FAIL"
        ),
    )

    for name, values in (
        ("low_middle", low_middle_angles),
        ("low_high", low_high_angles),
        ("middle_high", middle_high_angles),
    ):
        add_metric(
            metric_rows,
            "opening_angle_degrees",
            f"{name}_mean",
            float(np.mean(values)),
        )

        add_metric(
            metric_rows,
            "opening_angle_degrees",
            f"{name}_std",
            float(np.std(values)),
        )

        add_metric(
            metric_rows,
            "opening_angle_degrees",
            f"{name}_minimum",
            min(values),
        )

        add_metric(
            metric_rows,
            "opening_angle_degrees",
            f"{name}_maximum",
            max(values),
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

    add_histogram_rows(
        histogram_rows,
        "photon_cos_theta",
        photon_cos_result["counts"],
        photon_cos_result["edges"],
    )

    add_histogram_rows(
        histogram_rows,
        "photon_phi_radians",
        photon_phi_result["counts"],
        photon_phi_result["edges"],
    )

    add_histogram_rows(
        histogram_rows,
        "plane_normal_cos_theta_positive_hemisphere",
        plane_cos_result["counts"],
        plane_cos_result["edges"],
    )

    add_histogram_rows(
        histogram_rows,
        "plane_normal_phi_radians",
        plane_phi_result["counts"],
        plane_phi_result["edges"],
    )

    angle_edges = np.linspace(
        0.0,
        180.0,
        args.angle_bins + 1,
    )

    for name, values in (
        ("opening_angle_low_middle_deg", low_middle_angles),
        ("opening_angle_low_high_deg", low_high_angles),
        ("opening_angle_middle_high_deg", middle_high_angles),
        ("opening_angle_all_pairs_deg", all_opening_angles),
    ):
        counts, edges = np.histogram(
            np.asarray(values),
            bins=angle_edges,
        )

        add_histogram_rows(
            histogram_rows,
            name,
            counts,
            edges,
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

    print("=== Three-Gamma Geometry Validation ===")
    print(f"Input CSV              : {args.gammas}")
    print(f"Events read            : {len(events)}")
    print(f"Events analyzed        : {valid_event_count}")
    print(f"Multiplicity failures  : {multiplicity_failures}")
    print(f"Plane-normal failures  : {plane_normal_failures}")
    print()

    print("Isotropy")
    print(
        "  photon cos(theta) z  : "
        f"{photon_cos_result['z_score']:.6f}"
    )
    print(
        "  photon phi z         : "
        f"{photon_phi_result['z_score']:.6f}"
    )
    print(
        "  plane cos(theta) z   : "
        f"{plane_cos_result['z_score']:.6f}"
    )
    print(
        "  plane phi z          : "
        f"{plane_phi_result['z_score']:.6f}"
    )
    print()

    print("Cartesian-axis means")
    print(f"  mean |x|             : {mean_abs_x:.8f}")
    print(f"  mean |y|             : {mean_abs_y:.8f}")
    print(f"  mean |z|             : {mean_abs_z:.8f}")
    print(f"  maximum difference   : {axis_asymmetry:.8f}")
    print()

    print("Exact geometry")
    print(
        "  max coplanarity error: "
        f"{max_coplanarity:.6e}"
    )
    print(
        "  max energy-sum error : "
        f"{max_energy_sum_error:.6e} MeV"
    )
    print(
        "  max momentum error   : "
        f"{max_momentum_error:.6e} MeV/c"
    )
    print()

    print(f"Summary JSON           : {summary_path}")
    print(f"Metrics CSV            : {metrics_path}")
    print(f"Histograms CSV         : {histograms_path}")
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
