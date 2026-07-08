#!/usr/bin/env python3

import argparse
import csv
from collections import defaultdict
from pathlib import Path

import numpy as np


ELECTRON_MASS_MEV = 0.51099895


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Export PsSource/native-Geant4/GATE Ore-Powell "
            "comparison data in Excel-friendly wide CSV format."
        )
    )

    parser.add_argument(
        "--pssource",
        required=True,
        type=Path,
    )
    parser.add_argument(
        "--native-geant4",
        required=True,
        type=Path,
    )
    parser.add_argument(
        "--gate",
        required=True,
        type=Path,
    )
    parser.add_argument(
        "--output-dir",
        required=True,
        type=Path,
    )
    parser.add_argument(
        "--bins",
        type=int,
        default=80,
    )
    parser.add_argument(
        "--cdf-points",
        type=int,
        default=1001,
    )

    return parser.parse_args()


def read_model(path):
    events = defaultdict(list)

    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)

        required = {
            "event_id",
            "kinetic_energy_MeV",
        }

        missing = required - set(reader.fieldnames or [])

        if missing:
            raise RuntimeError(
                f"{path}: missing columns {sorted(missing)}"
            )

        for row in reader:
            event_id = int(row["event_id"])
            energy = float(row["kinetic_energy_MeV"])
            events[event_id].append(
                energy / ELECTRON_MASS_MEV
            )

    bad = {
        event_id: len(values)
        for event_id, values in events.items()
        if len(values) != 3
    }

    if bad:
        examples = list(sorted(bad.items()))[:20]
        raise RuntimeError(
            f"{path}: events do not all contain three photons: "
            f"{examples}"
        )

    ordered = np.asarray(
        [
            sorted(events[event_id])
            for event_id in sorted(events)
        ],
        dtype=float,
    )

    unordered = ordered.reshape(-1)

    return {
        "event_count": ordered.shape[0],
        "photon_count": unordered.size,
        "unordered": unordered,
        "low": ordered[:, 0],
        "middle": ordered[:, 1],
        "high": ordered[:, 2],
    }


def histogram_density(values, edges):
    counts, _ = np.histogram(
        values,
        bins=edges,
    )

    widths = np.diff(edges)

    return counts / (
        values.size * widths
    )


def empirical_cdf(values, grid):
    sorted_values = np.sort(values)

    return np.searchsorted(
        sorted_values,
        grid,
        side="right",
    ) / sorted_values.size


def write_csv(path, fieldnames, rows):
    with path.open(
        "w",
        newline="",
        encoding="utf-8",
    ) as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=fieldnames,
        )
        writer.writeheader()
        writer.writerows(rows)


def main():
    args = parse_args()

    for path in (
        args.pssource,
        args.native_geant4,
        args.gate,
    ):
        if not path.is_file():
            raise FileNotFoundError(path)

    if args.bins <= 0:
        raise ValueError("--bins must be positive")

    if args.cdf_points < 2:
        raise ValueError("--cdf-points must be at least 2")

    args.output_dir.mkdir(
        parents=True,
        exist_ok=True,
    )

    models = {
        "pssource": read_model(args.pssource),
        "native_geant4": read_model(
            args.native_geant4
        ),
        "gate": read_model(args.gate),
    }

    labels = {
        "pssource": "PsSource",
        "native_geant4": "Native Geant4",
        "gate": "GATE",
    }

    edges = np.linspace(
        0.0,
        1.0,
        args.bins + 1,
    )

    centers = 0.5 * (
        edges[:-1] + edges[1:]
    )

    # ----------------------------------------------------------
    # Unordered single-photon spectrum
    # ----------------------------------------------------------

    unordered_density = {
        key: histogram_density(
            model["unordered"],
            edges,
        )
        for key, model in models.items()
    }

    unordered_rows = []

    for i, center in enumerate(centers):
        unordered_rows.append(
            {
                "x_left": f"{edges[i]:.10f}",
                "x_right": f"{edges[i + 1]:.10f}",
                "x_center": f"{center:.10f}",
                "PsSource_density": (
                    f"{unordered_density['pssource'][i]:.12g}"
                ),
                "Native_Geant4_density": (
                    f"{unordered_density['native_geant4'][i]:.12g}"
                ),
                "GATE_density": (
                    f"{unordered_density['gate'][i]:.12g}"
                ),
            }
        )

    write_csv(
        args.output_dir
        / "excel_unordered_spectrum.csv",
        [
            "x_left",
            "x_right",
            "x_center",
            "PsSource_density",
            "Native_Geant4_density",
            "GATE_density",
        ],
        unordered_rows,
    )

    # ----------------------------------------------------------
    # Ordered low-, middle-, and high-energy spectra
    # ----------------------------------------------------------

    ordered_density = {}

    for key, model in models.items():
        for rank in ("low", "middle", "high"):
            ordered_density[(key, rank)] = (
                histogram_density(
                    model[rank],
                    edges,
                )
            )

    ordered_rows = []

    for i, center in enumerate(centers):
        ordered_rows.append(
            {
                "x_left": f"{edges[i]:.10f}",
                "x_right": f"{edges[i + 1]:.10f}",
                "x_center": f"{center:.10f}",
                "PsSource_low_density": (
                    f"{ordered_density[('pssource', 'low')][i]:.12g}"
                ),
                "Native_Geant4_low_density": (
                    f"{ordered_density[('native_geant4', 'low')][i]:.12g}"
                ),
                "GATE_low_density": (
                    f"{ordered_density[('gate', 'low')][i]:.12g}"
                ),
                "PsSource_middle_density": (
                    f"{ordered_density[('pssource', 'middle')][i]:.12g}"
                ),
                "Native_Geant4_middle_density": (
                    f"{ordered_density[('native_geant4', 'middle')][i]:.12g}"
                ),
                "GATE_middle_density": (
                    f"{ordered_density[('gate', 'middle')][i]:.12g}"
                ),
                "PsSource_high_density": (
                    f"{ordered_density[('pssource', 'high')][i]:.12g}"
                ),
                "Native_Geant4_high_density": (
                    f"{ordered_density[('native_geant4', 'high')][i]:.12g}"
                ),
                "GATE_high_density": (
                    f"{ordered_density[('gate', 'high')][i]:.12g}"
                ),
            }
        )

    write_csv(
        args.output_dir
        / "excel_ordered_spectra.csv",
        list(ordered_rows[0].keys()),
        ordered_rows,
    )

    # ----------------------------------------------------------
    # Unordered empirical CDF and pairwise CDF differences
    # ----------------------------------------------------------

    cdf_grid = np.linspace(
        0.0,
        1.0,
        args.cdf_points,
    )

    cdf = {
        key: empirical_cdf(
            model["unordered"],
            cdf_grid,
        )
        for key, model in models.items()
    }

    cdf_rows = []

    for i, x_value in enumerate(cdf_grid):
        cdf_rows.append(
            {
                "x": f"{x_value:.10f}",
                "PsSource_CDF": (
                    f"{cdf['pssource'][i]:.12g}"
                ),
                "Native_Geant4_CDF": (
                    f"{cdf['native_geant4'][i]:.12g}"
                ),
                "GATE_CDF": (
                    f"{cdf['gate'][i]:.12g}"
                ),
                "PsSource_minus_Native_Geant4": (
                    f"{cdf['pssource'][i] - cdf['native_geant4'][i]:.12g}"
                ),
                "PsSource_minus_GATE": (
                    f"{cdf['pssource'][i] - cdf['gate'][i]:.12g}"
                ),
                "Native_Geant4_minus_GATE": (
                    f"{cdf['native_geant4'][i] - cdf['gate'][i]:.12g}"
                ),
            }
        )

    write_csv(
        args.output_dir
        / "excel_unordered_cdf.csv",
        list(cdf_rows[0].keys()),
        cdf_rows,
    )

    # ----------------------------------------------------------
    # Model summary
    # ----------------------------------------------------------

    summary_rows = []

    for key, model in models.items():
        summary_rows.append(
            {
                "model": labels[key],
                "input_csv": str(
                    {
                        "pssource": args.pssource,
                        "native_geant4": (
                            args.native_geant4
                        ),
                        "gate": args.gate,
                    }[key]
                ),
                "event_count": model["event_count"],
                "photon_count": model["photon_count"],
                "mean_unordered_x": (
                    f"{np.mean(model['unordered']):.12g}"
                ),
                "std_unordered_x": (
                    f"{np.std(model['unordered']):.12g}"
                ),
                "mean_low_x": (
                    f"{np.mean(model['low']):.12g}"
                ),
                "mean_middle_x": (
                    f"{np.mean(model['middle']):.12g}"
                ),
                "mean_high_x": (
                    f"{np.mean(model['high']):.12g}"
                ),
            }
        )

    write_csv(
        args.output_dir
        / "excel_model_summary.csv",
        list(summary_rows[0].keys()),
        summary_rows,
    )

    # ----------------------------------------------------------
    # Pairwise metrics from the authoritative comparison
    # ----------------------------------------------------------

    pairwise_source = (
        Path(__file__).resolve().parent
        / "cross_framework_ore_powell_run"
        / "manuscript_pairwise_summary.csv"
    )

    if pairwise_source.is_file():
        with pairwise_source.open(
            newline="",
            encoding="utf-8",
        ) as handle:
            pairwise_rows = list(
                csv.DictReader(handle)
            )

        display_names = {
            "pssource": "PsSource",
            "native_geant4": "Native Geant4",
            "gate": "GATE",
        }

        formatted_rows = []

        for row in pairwise_rows:
            left, right = row["comparison"].split(
                " vs "
            )

            formatted_rows.append(
                {
                    "comparison": (
                        f"{display_names[left]} vs "
                        f"{display_names[right]}"
                    ),
                    "unordered_marginal_KS": (
                        row["unordered_marginal_ks"]
                    ),
                    "largest_ordered_energy_KS": (
                        row[
                            "largest_ordered_energy_ks"
                        ]
                    ),
                    "unordered_histogram_L1": (
                        row["unordered_histogram_l1"]
                    ),
                }
            )

        write_csv(
            args.output_dir
            / "excel_pairwise_metrics.csv",
            list(formatted_rows[0].keys()),
            formatted_rows,
        )

    print("=== Excel Data Export PASS ===")
    print(f"Output directory: {args.output_dir}")
    print()
    print("Generated:")
    for path in sorted(
        args.output_dir.glob("excel_*.csv")
    ):
        print(f"  {path.name}")


if __name__ == "__main__":
    main()
