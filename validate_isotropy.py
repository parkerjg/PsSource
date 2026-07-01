#!/usr/bin/env python3

import csv
import math
import statistics
import sys
from pathlib import Path


INPUT = Path("annihilation_gammas.csv")


def main() -> int:
    if not INPUT.exists():
        print(f"ERROR: missing {INPUT}", file=sys.stderr)
        return 1

    x_values = []
    y_values = []
    z_values = []

    with INPUT.open(newline="") as handle:
        reader = csv.DictReader(handle)

        required = {"dir_x", "dir_y", "dir_z"}
        missing = required - set(reader.fieldnames or [])

        if missing:
            print(
                f"ERROR: missing columns: {sorted(missing)}",
                file=sys.stderr
            )
            return 1

        for row in reader:
            x_values.append(float(row["dir_x"]))
            y_values.append(float(row["dir_y"]))
            z_values.append(float(row["dir_z"]))

    if not x_values:
        print("ERROR: no photon directions found", file=sys.stderr)
        return 1

    def summarize(values):
        return {
            "mean": statistics.mean(values),
            "mean_square": statistics.mean(v * v for v in values),
            "positive_fraction": sum(v > 0.0 for v in values) / len(values),
            "exact_zero_count": sum(v == 0.0 for v in values),
        }

    sx = summarize(x_values)
    sy = summarize(y_values)
    sz = summarize(z_values)

    print("=== Direction Isotropy Check ===")
    print(f"Photon count: {len(x_values)}")
    print()

    for name, result in (
        ("x", sx),
        ("y", sy),
        ("z", sz),
    ):
        print(f"{name}:")
        print(f"  mean              = {result['mean']:.8f}")
        print(f"  mean square       = {result['mean_square']:.8f}")
        print(f"  positive fraction = {result['positive_fraction']:.8f}")
        print(f"  exact zero count  = {result['exact_zero_count']}")

    expected_mean_square = 1.0 / 3.0

    maximum_absolute_mean = max(
        abs(sx["mean"]),
        abs(sy["mean"]),
        abs(sz["mean"]),
    )

    maximum_mean_square_deviation = max(
        abs(sx["mean_square"] - expected_mean_square),
        abs(sy["mean_square"] - expected_mean_square),
        abs(sz["mean_square"] - expected_mean_square),
    )

    maximum_sign_imbalance = max(
        abs(sx["positive_fraction"] - 0.5),
        abs(sy["positive_fraction"] - 0.5),
        abs(sz["positive_fraction"] - 0.5),
    )

    print()
    print(f"Maximum |component mean|       = {maximum_absolute_mean:.8f}")
    print(
        "Maximum |<component²> - 1/3| = "
        f"{maximum_mean_square_deviation:.8f}"
    )
    print(f"Maximum sign imbalance         = {maximum_sign_imbalance:.8f}")

    # Loose smoke-test thresholds for approximately 3000 directions.
    passed = (
        maximum_absolute_mean < 0.05
        and maximum_mean_square_deviation < 0.05
        and maximum_sign_imbalance < 0.05
        and sx["exact_zero_count"] == 0
        and sy["exact_zero_count"] == 0
        and sz["exact_zero_count"] == 0
    )

    print()
    print("PASS" if passed else "FAIL")

    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
