#!/usr/bin/env python3

import argparse
import csv
import math
import sys
from pathlib import Path


REQUIRED_COLUMNS = {
    "event_id",
    "dir_x",
    "dir_y",
    "dir_z",
    "pol_x",
    "pol_y",
    "pol_z",
    "polarization_valid",
}


def magnitude(x: float, y: float, z: float) -> float:
    return math.sqrt(x * x + y * y + z * z)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate polarization fields in annihilation_gammas.csv."
    )

    parser.add_argument(
        "csv_file",
        nargs="?",
        default="annihilation_gammas.csv",
        help="Photon CSV to validate.",
    )

    parser.add_argument(
        "--expect-polarized",
        action="store_true",
        help="Require every photon to have valid nonzero polarization.",
    )

    parser.add_argument(
        "--expect-unpolarized",
        action="store_true",
        help="Require every photon to have zero invalid polarization.",
    )

    parser.add_argument(
        "--norm-tolerance",
        type=float,
        default=2.0e-5,
        help="Allowed deviation of direction and polarization magnitudes from 1.",
    )

    parser.add_argument(
        "--orthogonality-tolerance",
        type=float,
        default=2.0e-5,
        help="Maximum allowed absolute dot product between direction and polarization.",
    )

    args = parser.parse_args()

    if args.expect_polarized and args.expect_unpolarized:
        parser.error(
            "--expect-polarized and --expect-unpolarized are mutually exclusive"
        )

    csv_path = Path(args.csv_file)

    if not csv_path.is_file():
        print(f"ERROR: file not found: {csv_path}", file=sys.stderr)
        return 1

    photon_count = 0
    valid_count = 0
    invalid_count = 0

    max_direction_norm_error = 0.0
    max_polarization_norm_error = 0.0
    max_abs_dot = 0.0
    max_invalid_polarization_magnitude = 0.0

    failures = []

    with csv_path.open(newline="") as handle:
        reader = csv.DictReader(handle)

        if reader.fieldnames is None:
            print("ERROR: CSV has no header.", file=sys.stderr)
            return 1

        missing = REQUIRED_COLUMNS.difference(reader.fieldnames)

        if missing:
            print(
                "ERROR: missing required columns: "
                + ", ".join(sorted(missing)),
                file=sys.stderr,
            )
            return 1

        for row_number, row in enumerate(reader, start=2):
            photon_count += 1

            try:
                direction = (
                    float(row["dir_x"]),
                    float(row["dir_y"]),
                    float(row["dir_z"]),
                )

                polarization = (
                    float(row["pol_x"]),
                    float(row["pol_y"]),
                    float(row["pol_z"]),
                )

                valid = int(row["polarization_valid"]) != 0

            except (TypeError, ValueError) as error:
                failures.append(
                    f"row {row_number}: invalid numeric field: {error}"
                )
                continue

            direction_norm = magnitude(*direction)

            polarization_norm = magnitude(*polarization)

            direction_norm_error = abs(direction_norm - 1.0)

            max_direction_norm_error = max(
                max_direction_norm_error,
                direction_norm_error,
            )

            if direction_norm_error > args.norm_tolerance:
                failures.append(
                    f"row {row_number}: direction norm "
                    f"{direction_norm:.9g} differs from 1"
                )

            if valid:
                valid_count += 1

                polarization_norm_error = abs(
                    polarization_norm - 1.0
                )

                max_polarization_norm_error = max(
                    max_polarization_norm_error,
                    polarization_norm_error,
                )

                dot_product = (
                    direction[0] * polarization[0]
                    + direction[1] * polarization[1]
                    + direction[2] * polarization[2]
                )

                max_abs_dot = max(
                    max_abs_dot,
                    abs(dot_product),
                )

                if polarization_norm_error > args.norm_tolerance:
                    failures.append(
                        f"row {row_number}: polarization norm "
                        f"{polarization_norm:.9g} differs from 1"
                    )

                if abs(dot_product) > args.orthogonality_tolerance:
                    failures.append(
                        f"row {row_number}: direction-polarization "
                        f"dot product is {dot_product:.9g}"
                    )

            else:
                invalid_count += 1

                max_invalid_polarization_magnitude = max(
                    max_invalid_polarization_magnitude,
                    polarization_norm,
                )

                if polarization_norm > args.norm_tolerance:
                    failures.append(
                        f"row {row_number}: polarization marked invalid "
                        f"but magnitude is {polarization_norm:.9g}"
                    )

    if photon_count == 0:
        failures.append("CSV contains no photon rows")

    if args.expect_polarized and valid_count != photon_count:
        failures.append(
            f"expected all photons polarized, but only "
            f"{valid_count}/{photon_count} were valid"
        )

    if args.expect_unpolarized and valid_count != 0:
        failures.append(
            f"expected no polarized photons, but "
            f"{valid_count}/{photon_count} were valid"
        )

    print("Polarization validation summary")
    print(f"  file: {csv_path}")
    print(f"  photons: {photon_count}")
    print(f"  polarization valid: {valid_count}")
    print(f"  polarization invalid: {invalid_count}")
    print(
        "  maximum direction norm error: "
        f"{max_direction_norm_error:.6e}"
    )
    print(
        "  maximum polarization norm error: "
        f"{max_polarization_norm_error:.6e}"
    )
    print(
        "  maximum |direction dot polarization|: "
        f"{max_abs_dot:.6e}"
    )
    print(
        "  maximum invalid polarization magnitude: "
        f"{max_invalid_polarization_magnitude:.6e}"
    )

    if failures:
        print()
        print("FAIL")

        for failure in failures[:20]:
            print(f"  - {failure}")

        if len(failures) > 20:
            print(
                f"  - ...and {len(failures) - 20} additional failures"
            )

        return 1

    print()
    print("PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main()) 
