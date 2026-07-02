#!/usr/bin/env python3

import argparse
import csv
import json
import math
import sys
from collections import defaultdict
from pathlib import Path

import numpy as np


ELECTRON_MASS_MEV = 0.51099895


def ore_powell_density(r1, r2):
    """
    Evaluate the ordinary Ore-Powell density used for unpolarized
    three-photon annihilation.

    Parameters may be scalars or NumPy arrays.

    Normalized photon energies:
        ri = Ei / (m_e c^2)

    with:
        r1 + r2 + r3 = 2
        0 < ri < 1
    """
    r1 = np.asarray(r1, dtype=float)
    r2 = np.asarray(r2, dtype=float)
    r3 = 2.0 - r1 - r2

    physical = (
        (r1 > 0.0)
        & (r1 < 1.0)
        & (r2 > 0.0)
        & (r2 < 1.0)
        & (r3 > 0.0)
        & (r3 < 1.0)
    )

    density = np.zeros(np.broadcast(r1, r2).shape, dtype=float)

    with np.errstate(divide="ignore", invalid="ignore"):
        cos12 = (
            r3 * r3 - r1 * r1 - r2 * r2
        ) / (2.0 * r1 * r2)

        cos13 = (
            r2 * r2 - r1 * r1 - r3 * r3
        ) / (2.0 * r1 * r3)

        cos23 = (
            r1 * r1 - r2 * r2 - r3 * r3
        ) / (2.0 * r2 * r3)

        valid_cosines = (
            (np.abs(cos12) <= 1.0 + 1.0e-12)
            & (np.abs(cos13) <= 1.0 + 1.0e-12)
            & (np.abs(cos23) <= 1.0 + 1.0e-12)
        )

        valid = physical & valid_cosines

        density[valid] = (
            (1.0 - cos12[valid]) ** 2
            + (1.0 - cos13[valid]) ** 2
            + (1.0 - cos23[valid]) ** 2
        )

    return density


def vector_dot(a, b):
    return (
        a[0] * b[0]
        + a[1] * b[1]
        + a[2] * b[2]
    )


def predicted_cosine(ri, rj, rk):
    return (
        rk * rk - ri * ri - rj * rj
    ) / (2.0 * ri * rj)


def load_events(csv_path):
    events = defaultdict(list)

    required_columns = {
        "event_id",
        "track_id",
        "kinetic_energy_MeV",
        "dir_x",
        "dir_y",
        "dir_z",
    }

    with csv_path.open(newline="") as handle:
        reader = csv.DictReader(handle)

        if reader.fieldnames is None:
            raise ValueError("CSV has no header")

        missing = required_columns.difference(reader.fieldnames)

        if missing:
            raise ValueError(
                "Missing required columns: "
                + ", ".join(sorted(missing))
            )

        for row_number, row in enumerate(reader, start=2):
            try:
                event_id = int(row["event_id"])
                track_id = int(row["track_id"])

                energy = float(row["kinetic_energy_MeV"])

                direction = (
                    float(row["dir_x"]),
                    float(row["dir_y"]),
                    float(row["dir_z"]),
                )

            except (TypeError, ValueError) as error:
                raise ValueError(
                    f"Invalid data at CSV row {row_number}: {error}"
                ) from error

            events[event_id].append(
                {
                    "track_id": track_id,
                    "energy": energy,
                    "direction": direction,
                }
            )

    return events


def build_reference_grid(bin_count, subdivisions):
    edges = np.linspace(0.0, 1.0, bin_count + 1)

    expected_mass = np.zeros(
        (bin_count, bin_count),
        dtype=float,
    )

    fractions = (
        np.arange(subdivisions, dtype=float) + 0.5
    ) / subdivisions

    for i in range(bin_count):
        r1_samples = (
            edges[i]
            + fractions * (edges[i + 1] - edges[i])
        )

        for j in range(bin_count):
            r2_samples = (
                edges[j]
                + fractions * (edges[j + 1] - edges[j])
            )

            r1_grid, r2_grid = np.meshgrid(
                r1_samples,
                r2_samples,
                indexing="ij",
            )

            density = ore_powell_density(
                r1_grid,
                r2_grid,
            )

            expected_mass[i, j] = density.mean()

    total_mass = expected_mass.sum()

    if not np.isfinite(total_mass) or total_mass <= 0.0:
        raise RuntimeError(
            "Analytic reference grid has zero or invalid mass"
        )

    expected_probability = expected_mass / total_mass

    return edges, expected_probability


def write_grid_csv(
    output_path,
    edges,
    observed,
    expected,
):
    with output_path.open("w", newline="") as handle:
        writer = csv.writer(handle)

        writer.writerow(
            [
                "r1_low",
                "r1_high",
                "r2_low",
                "r2_high",
                "observed_count",
                "expected_probability",
                "expected_count",
                "standardized_residual",
            ]
        )

        event_count = observed.sum()

        for i in range(observed.shape[0]):
            for j in range(observed.shape[1]):
                expected_count = (
                    expected[i, j] * event_count
                )

                if expected_count > 0.0:
                    residual = (
                        observed[i, j] - expected_count
                    ) / math.sqrt(expected_count)
                else:
                    residual = math.nan

                writer.writerow(
                    [
                        edges[i],
                        edges[i + 1],
                        edges[j],
                        edges[j + 1],
                        int(observed[i, j]),
                        expected[i, j],
                        expected_count,
                        residual,
                    ]
                )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Independently validate Geant4 Ore-Powell "
            "three-photon output against the analytic density."
        )
    )

    parser.add_argument(
        "csv_file",
        nargs="?",
        default="annihilation_gammas.csv",
        help="Photon-level CSV file.",
    )

    parser.add_argument(
        "--bins",
        type=int,
        default=50,
        help="Number of bins per normalized-energy axis.",
    )

    parser.add_argument(
        "--subdivisions",
        type=int,
        default=12,
        help=(
            "Integration subdivisions per dimension "
            "inside each analytic grid bin."
        ),
    )

    parser.add_argument(
        "--minimum-expected-count",
        type=float,
        default=5.0,
        help=(
            "Minimum expected count for inclusion "
            "in the Pearson chi-square statistic."
        ),
    )

    parser.add_argument(
        "--chi-square-sigma-limit",
        type=float,
        default=6.0,
        help=(
            "Maximum absolute normal-approximation z score "
            "for the Pearson chi-square statistic."
        ),
    )

    parser.add_argument(
        "--cdf-factor",
        type=float,
        default=6.0,
        help=(
            "Marginal CDF tolerance factor divided by "
            "sqrt(number of events)."
        ),
    )

    parser.add_argument(
        "--energy-sum-tolerance",
        type=float,
        default=5.0e-6,
        help=(
            "Maximum allowed normalized-energy sum error."
        ),
    )

    parser.add_argument(
        "--angle-tolerance",
        type=float,
        default=1.0e-5,
        help=(
            "Maximum allowed difference between measured and "
            "energy-predicted pairwise direction cosines."
        ),
    )

    parser.add_argument(
        "--angle-energy-floor",
        type=float,
        default=1.0e-3,
        help=(
            "Minimum normalized energy for both photons in an "
            "opening-angle comparison. Softer pairs are numerically "
            "unstable after CSV rounding."
        ),
    )

    parser.add_argument(
        "--output-prefix",
        default="ore_powell_reference",
        help="Prefix for JSON and grid CSV outputs.",
    )

    args = parser.parse_args()

    if args.bins < 5:
        parser.error("--bins must be at least 5")

    if args.subdivisions < 1:
        parser.error("--subdivisions must be positive")

    csv_path = Path(args.csv_file)

    if not csv_path.is_file():
        print(
            f"ERROR: file not found: {csv_path}",
            file=sys.stderr,
        )
        return 1

    try:
        event_map = load_events(csv_path)
    except ValueError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1

    labeled_r1 = []
    labeled_r2 = []

    event_count = 0
    multiplicity_failures = 0
    phase_space_failures = 0

    max_energy_sum_error = 0.0
    max_angle_cosine_error = 0.0
    angle_cosine_errors = []
    skipped_soft_angle_pairs = 0
    max_direction_norm_error = 0.0

    for event_id in sorted(event_map):
        photons = event_map[event_id]

        if len(photons) != 3:
            multiplicity_failures += 1
            continue

        # Track ID provides a deterministic photon labeling.
        # The ordinary Ore-Powell density is permutation symmetric,
        # so any consistent labeling is valid.
        photons.sort(
            key=lambda photon: photon["track_id"]
        )

        normalized_energies = [
            photon["energy"] / ELECTRON_MASS_MEV
            for photon in photons
        ]

        directions = [
            photon["direction"]
            for photon in photons
        ]

        energy_sum_error = abs(
            sum(normalized_energies) - 2.0
        )

        max_energy_sum_error = max(
            max_energy_sum_error,
            energy_sum_error,
        )

        physical = all(
            0.0 < energy < 1.0
            for energy in normalized_energies
        )

        if (
            not physical
            or energy_sum_error
            > args.energy_sum_tolerance
        ):
            phase_space_failures += 1

        for direction in directions:
            direction_norm = math.sqrt(
                vector_dot(direction, direction)
            )

            max_direction_norm_error = max(
                max_direction_norm_error,
                abs(direction_norm - 1.0),
            )

        pair_indices = (
            (0, 1, 2),
            (0, 2, 1),
            (1, 2, 0),
        )

        for i, j, k in pair_indices:
            if (
                normalized_energies[i]
                < args.angle_energy_floor
                or normalized_energies[j]
                < args.angle_energy_floor
            ):
                skipped_soft_angle_pairs += 1
                continue

            measured_cosine = vector_dot(
                directions[i],
                directions[j],
            )

            expected_cosine = predicted_cosine(
                normalized_energies[i],
                normalized_energies[j],
                normalized_energies[k],
            )

            cosine_error = abs(
                measured_cosine - expected_cosine
            )

            angle_cosine_errors.append(cosine_error)

            max_angle_cosine_error = max(
                max_angle_cosine_error,
                cosine_error,
            )


        labeled_r1.append(normalized_energies[0])
        labeled_r2.append(normalized_energies[1])

        event_count += 1

    if angle_cosine_errors:
        angle_error_array = np.asarray(
            angle_cosine_errors,
            dtype=float,
        )

        angle_error_p99 = float(
            np.percentile(angle_error_array, 99.0)
        )

        angle_error_p999 = float(
            np.percentile(angle_error_array, 99.9)
        )
    else:
        angle_error_p99 = math.nan
        angle_error_p999 = math.nan

    if event_count == 0:
        print(
            "ERROR: no valid three-photon events found",
            file=sys.stderr,
        )
        return 1

    r1_values = np.asarray(labeled_r1, dtype=float)
    r2_values = np.asarray(labeled_r2, dtype=float)

    edges, expected_probability = build_reference_grid(
        args.bins,
        args.subdivisions,
    )

    observed, _, _ = np.histogram2d(
        r1_values,
        r2_values,
        bins=(edges, edges),
    )

    expected_counts = (
        expected_probability * event_count
    )

    included = (
        expected_counts
        >= args.minimum_expected_count
    )

    observed_included = observed[included]
    expected_included = expected_counts[included]

    included_bin_count = int(included.sum())

    if included_bin_count == 0:
        print(
            "ERROR: no phase-space bins met "
            f"--minimum-expected-count={args.minimum_expected_count}. "
            "Increase the event count, reduce --bins, or lower "
            "--minimum-expected-count.",
            file=sys.stderr,
        )
        return 1

    chi_square = np.sum(
        (
            observed_included
            - expected_included
        ) ** 2
        / expected_included
    )

    degrees_of_freedom = included_bin_count - 1

    if degrees_of_freedom > 0:
        chi_square_z = (
            chi_square - degrees_of_freedom
        ) / math.sqrt(
            2.0 * degrees_of_freedom
        )
    else:
        chi_square_z = math.nan

    expected_r1_probability = (
        expected_probability.sum(axis=1)
    )

    expected_r1_cdf = np.cumsum(
        expected_r1_probability
    )

    observed_r1_counts, _ = np.histogram(
        r1_values,
        bins=edges,
    )

    observed_r1_cdf = np.cumsum(
        observed_r1_counts
    ) / event_count

    max_marginal_cdf_difference = float(
        np.max(
            np.abs(
                observed_r1_cdf
                - expected_r1_cdf
            )
        )
    )

    cdf_tolerance = (
        args.cdf_factor
        / math.sqrt(event_count)
    )

    maximum_standardized_residual = float(
        np.max(
            np.abs(
                (
                    observed_included
                    - expected_included
                )
                / np.sqrt(expected_included)
            )
        )
    )

    failures = []

    if multiplicity_failures != 0:
        failures.append(
            f"{multiplicity_failures} events did not contain "
            "exactly three photons"
        )

    if phase_space_failures != 0:
        failures.append(
            f"{phase_space_failures} events violated normalized "
            "three-photon phase space"
        )

    if (
        max_energy_sum_error
        > args.energy_sum_tolerance
    ):
        failures.append(
            "maximum normalized-energy sum error "
            f"{max_energy_sum_error:.6e} exceeded "
            f"{args.energy_sum_tolerance:.6e}"
        )

    if (
        not math.isfinite(angle_error_p999)
        or angle_error_p999
        > args.angle_tolerance
    ):
        failures.append(
            "99.9th-percentile opening-angle cosine error "
            f"{angle_error_p999:.6e} exceeded "
            f"{args.angle_tolerance:.6e}"
        )

    if (
        not math.isfinite(chi_square_z)
        or abs(chi_square_z)
        > args.chi_square_sigma_limit
    ):
        failures.append(
            "2D analytic-density chi-square z score "
            f"{chi_square_z:.6f} exceeded "
            f"±{args.chi_square_sigma_limit:.6f}"
        )

    if (
        max_marginal_cdf_difference
        > cdf_tolerance
    ):
        failures.append(
            "marginal energy CDF difference "
            f"{max_marginal_cdf_difference:.6e} exceeded "
            f"{cdf_tolerance:.6e}"
        )

    summary = {
        "input_csv": str(csv_path),
        "electron_mass_MeV": ELECTRON_MASS_MEV,
        "events_analyzed": event_count,
        "multiplicity_failures": multiplicity_failures,
        "phase_space_failures": phase_space_failures,
        "maximum_normalized_energy_sum_error": (
            max_energy_sum_error
        ),
        "maximum_direction_norm_error": (
            max_direction_norm_error
        ),
        "maximum_opening_angle_cosine_error": (
            max_angle_cosine_error
        ),
        "grid_bins_per_axis": args.bins,
        "integration_subdivisions_per_axis": (
            args.subdivisions
        ),
        "included_chi_square_bins": int(
            included.sum()
        ),
        "chi_square": float(chi_square),
        "degrees_of_freedom": degrees_of_freedom,
        "chi_square_z_score": float(chi_square_z),
        "angle_energy_floor": args.angle_energy_floor,
        "opening_angle_pairs_tested": len(angle_cosine_errors),
        "opening_angle_pairs_skipped_soft": skipped_soft_angle_pairs,
        "opening_angle_cosine_error_p99": angle_error_p99,
        "opening_angle_cosine_error_p999": angle_error_p999,
        "maximum_standardized_residual": (
            maximum_standardized_residual
        ),
        "maximum_marginal_cdf_difference": (
            max_marginal_cdf_difference
        ),
        "marginal_cdf_tolerance": cdf_tolerance,
        "status": "PASS" if not failures else "FAIL",
        "failures": failures,
    }

    output_prefix = Path(args.output_prefix)

    json_path = Path(
        str(output_prefix) + "_summary.json"
    )

    grid_path = Path(
        str(output_prefix) + "_grid.csv"
    )

    with json_path.open("w") as handle:
        json.dump(
            summary,
            handle,
            indent=2,
            sort_keys=True,
        )

        handle.write("\n")

    write_grid_csv(
        grid_path,
        edges,
        observed,
        expected_probability,
    )

    print("=== Independent Ore-Powell Reference Validation ===")
    print(f"Input CSV: {csv_path}")
    print(f"Events analyzed: {event_count}")
    print()
    print("Exact event-level kinematics")
    print(
        "  multiplicity failures             : "
        f"{multiplicity_failures}"
    )
    print(
        "  phase-space failures              : "
        f"{phase_space_failures}"
    )
    print(
        "  max normalized-energy sum error   : "
        f"{max_energy_sum_error:.6e}"
    )
    print(
        "  max direction norm error          : "
        f"{max_direction_norm_error:.6e}"
    )
    print(
        "  max opening-angle cosine error    : "
        f"{max_angle_cosine_error:.6e}"
    )
    print(
        "  99th-percentile angle error       : "
        f"{angle_error_p99:.6e}"
    )
    print(
        "  99.9th-percentile angle error     : "
        f"{angle_error_p999:.6e}"
    )
    print(
        "  soft angle pairs skipped          : "
        f"{skipped_soft_angle_pairs}"
    )
    print()
    print("Analytic 2D Ore-Powell density comparison")
    print(
        "  grid bins per axis                : "
        f"{args.bins}"
    )
    print(
        "  integration subdivisions per axis : "
        f"{args.subdivisions}"
    )
    print(
        "  chi-square bins included          : "
        f"{int(included.sum())}"
    )
    print(
        "  chi-square                        : "
        f"{chi_square:.6f}"
    )
    print(
        "  degrees of freedom                : "
        f"{degrees_of_freedom}"
    )
    print(
        "  chi-square z score                : "
        f"{chi_square_z:.6f}"
    )
    print(
        "  maximum standardized residual     : "
        f"{maximum_standardized_residual:.6f}"
    )
    print()
    print("One-photon marginal comparison")
    print(
        "  maximum CDF difference            : "
        f"{max_marginal_cdf_difference:.6e}"
    )
    print(
        "  CDF tolerance                     : "
        f"{cdf_tolerance:.6e}"
    )
    print()
    print(f"Summary JSON: {json_path}")
    print(f"Grid CSV    : {grid_path}")
    print()

    if failures:
        print("FAIL")

        for failure in failures:
            print(f"  - {failure}")

        return 1

    print("PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
