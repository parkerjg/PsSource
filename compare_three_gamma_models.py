#!/usr/bin/env python3

"""
Compare positronium three-gamma energy distributions.

This script reads one or more annihilation_gammas.csv files and compares:

- photon-energy support;
- event multiplicity;
- per-event energy conservation;
- normalized photon energies x = E_gamma / (m_e c^2);
- unordered single-photon marginal distributions;
- ordered low-, middle-, and high-energy photon distributions;
- pairwise empirical Kolmogorov-Smirnov distances;
- pairwise histogram L1 distances.

This is a model-comparison and regression-validation script.

It does not yet constitute an independent analytic validation of the
Ore-Powell spectrum. That requires comparison with an analytic QED
reference distribution or an independently implemented trusted sampler.

Example
-------

python compare_three_gamma_models.py \
    --input approximate:results_approximate/annihilation_gammas.csv \
    --input ore_powell:results_ore_powell/annihilation_gammas.csv \
    --input polarized:results_polarized/annihilation_gammas.csv \
    --output-prefix three_gamma_comparison
"""

from __future__ import annotations

import argparse
import csv
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

try:
    import matplotlib.pyplot as plt
except ImportError:
    plt = None


ELECTRON_MASS_MEV = 0.51099895
EXPECTED_TOTAL_ENERGY_MEV = 2.0 * ELECTRON_MASS_MEV

CSV_ENERGY_PRECISION_MEV = 1.0e-6
NORMALIZED_SUPPORT_TOLERANCE = (
    CSV_ENERGY_PRECISION_MEV /
    ELECTRON_MASS_MEV
)

REQUIRED_COLUMNS = {
    "event_id",
    "kinetic_energy_MeV",
}


@dataclass
class ModelData:
    label: str
    path: Path

    event_ids: list[int]
    event_energies_MeV: list[tuple[float, float, float]]

    all_energies_MeV: list[float]
    all_x: list[float]

    ordered_low_x: list[float]
    ordered_mid_x: list[float]
    ordered_high_x: list[float]

    event_energy_errors_MeV: list[float]


def parse_input_spec(text: str) -> tuple[str, Path]:
    if ":" not in text:
        raise argparse.ArgumentTypeError(
            "Each --input must have the form LABEL:PATH"
        )

    label, path_text = text.split(":", 1)

    label = label.strip()
    path_text = path_text.strip()

    if not label:
        raise argparse.ArgumentTypeError(
            "Input label cannot be empty."
        )

    if not path_text:
        raise argparse.ArgumentTypeError(
            "Input path cannot be empty."
        )

    return label, Path(path_text)


def read_three_gamma_csv(
    label: str,
    path: Path,
) -> ModelData:
    if not path.exists():
        raise FileNotFoundError(
            f"Input file does not exist: {path}"
        )

    energies_by_event: dict[int, list[float]] = {}

    with path.open("r", newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)

        if reader.fieldnames is None:
            raise ValueError(
                f"{path}: CSV file has no header."
            )

        missing_columns = (
            REQUIRED_COLUMNS - set(reader.fieldnames)
        )

        if missing_columns:
            raise ValueError(
                f"{path}: missing required columns: "
                f"{sorted(missing_columns)}"
            )

        for line_number, row in enumerate(reader, start=2):
            try:
                event_id = int(row["event_id"])
                energy_MeV = float(
                    row["kinetic_energy_MeV"]
                )
            except (TypeError, ValueError) as exc:
                raise ValueError(
                    f"{path}:{line_number}: invalid event ID "
                    f"or photon energy."
                ) from exc

            if not math.isfinite(energy_MeV):
                raise ValueError(
                    f"{path}:{line_number}: non-finite energy."
                )

            energies_by_event.setdefault(
                event_id,
                [],
            ).append(energy_MeV)

    if not energies_by_event:
        raise ValueError(
            f"{path}: no photon rows were found."
        )

    event_ids: list[int] = []
    event_energies: list[tuple[float, float, float]] = []

    invalid_multiplicity: list[tuple[int, int]] = []

    for event_id in sorted(energies_by_event):
        energies = energies_by_event[event_id]

        if len(energies) != 3:
            invalid_multiplicity.append(
                (event_id, len(energies))
            )
            continue

        event_ids.append(event_id)
        event_energies.append(
            (
                energies[0],
                energies[1],
                energies[2],
            )
        )

    if invalid_multiplicity:
        examples = ", ".join(
            f"event {event_id}: {count} photons"
            for event_id, count in invalid_multiplicity[:10]
        )

        raise ValueError(
            f"{path}: found events without exactly three photons. "
            f"Examples: {examples}"
        )

    all_energies: list[float] = []
    all_x: list[float] = []

    ordered_low_x: list[float] = []
    ordered_mid_x: list[float] = []
    ordered_high_x: list[float] = []

    energy_errors: list[float] = []

    for energies in event_energies:
        all_energies.extend(energies)

        normalized = [
            energy / ELECTRON_MASS_MEV
            for energy in energies
        ]

        all_x.extend(normalized)

        ordered = sorted(normalized)

        ordered_low_x.append(ordered[0])
        ordered_mid_x.append(ordered[1])
        ordered_high_x.append(ordered[2])

        energy_errors.append(
            sum(energies) -
            EXPECTED_TOTAL_ENERGY_MEV
        )

    return ModelData(
        label=label,
        path=path,
        event_ids=event_ids,
        event_energies_MeV=event_energies,
        all_energies_MeV=all_energies,
        all_x=all_x,
        ordered_low_x=ordered_low_x,
        ordered_mid_x=ordered_mid_x,
        ordered_high_x=ordered_high_x,
        event_energy_errors_MeV=energy_errors,
    )


def mean(values: Sequence[float]) -> float:
    if not values:
        return math.nan

    return sum(values) / len(values)


def sample_std(values: Sequence[float]) -> float:
    if len(values) < 2:
        return 0.0

    center = mean(values)

    variance = sum(
        (value - center) ** 2
        for value in values
    ) / (len(values) - 1)

    return math.sqrt(variance)

def standard_error(values: Sequence[float]) -> float:
    if len(values) < 2:
        return math.nan

    return sample_std(values) / math.sqrt(len(values))

def quantile(
    values: Sequence[float],
    probability: float,
) -> float:
    if not values:
        return math.nan

    if probability <= 0.0:
        return min(values)

    if probability >= 1.0:
        return max(values)

    ordered = sorted(values)

    position = probability * (len(ordered) - 1)

    lower_index = int(math.floor(position))
    upper_index = int(math.ceil(position))

    if lower_index == upper_index:
        return ordered[lower_index]

    fraction = position - lower_index

    return (
        ordered[lower_index] * (1.0 - fraction) +
        ordered[upper_index] * fraction
    )


def empirical_ks_distance(
    values_a: Sequence[float],
    values_b: Sequence[float],
) -> float:
    """
    Compute the two-sample empirical KS distance without SciPy.
    """

    if not values_a or not values_b:
        return math.nan

    sorted_a = sorted(values_a)
    sorted_b = sorted(values_b)

    index_a = 0
    index_b = 0

    count_a = len(sorted_a)
    count_b = len(sorted_b)

    maximum_distance = 0.0

    while index_a < count_a or index_b < count_b:
        if index_a >= count_a:
            current_value = sorted_b[index_b]
        elif index_b >= count_b:
            current_value = sorted_a[index_a]
        else:
            current_value = min(
                sorted_a[index_a],
                sorted_b[index_b],
            )

        while (
            index_a < count_a and
            sorted_a[index_a] <= current_value
        ):
            index_a += 1

        while (
            index_b < count_b and
            sorted_b[index_b] <= current_value
        ):
            index_b += 1

        cdf_a = index_a / count_a
        cdf_b = index_b / count_b

        maximum_distance = max(
            maximum_distance,
            abs(cdf_a - cdf_b),
        )

    return maximum_distance


def histogram_probabilities(
    values: Sequence[float],
    bin_edges: Sequence[float],
) -> list[float]:
    counts = [0] * (len(bin_edges) - 1)

    for value in values:
        if value < bin_edges[0]:
            continue

        if value > bin_edges[-1]:
            continue

        if value == bin_edges[-1]:
            bin_index = len(counts) - 1
        else:
            width = (
                bin_edges[-1] - bin_edges[0]
            ) / len(counts)

            bin_index = int(
                (value - bin_edges[0]) / width
            )

            bin_index = min(
                max(bin_index, 0),
                len(counts) - 1,
            )

        counts[bin_index] += 1

    total = sum(counts)

    if total == 0:
        return [0.0] * len(counts)

    return [
        count / total
        for count in counts
    ]


def histogram_l1_distance(
    values_a: Sequence[float],
    values_b: Sequence[float],
    bin_edges: Sequence[float],
) -> float:
    probability_a = histogram_probabilities(
        values_a,
        bin_edges,
    )

    probability_b = histogram_probabilities(
        values_b,
        bin_edges,
    )

    return sum(
        abs(a - b)
        for a, b in zip(
            probability_a,
            probability_b,
        )
    )


def make_uniform_bin_edges(
    minimum: float,
    maximum: float,
    bin_count: int,
) -> list[float]:
    if bin_count <= 0:
        raise ValueError(
            "Bin count must be positive."
        )

    width = (maximum - minimum) / bin_count

    return [
        minimum + index * width
        for index in range(bin_count + 1)
    ]


def print_model_summary(data: ModelData) -> None:
    absolute_errors = [
        abs(value)
        for value in data.event_energy_errors_MeV
    ]

    support_violations = [
        value
        for value in data.all_x
        if (
            value <= 0.0 or
            value > 1.0 + NORMALIZED_SUPPORT_TOLERANCE
        )
    ]

    print()
    print(f"=== {data.label} ===")
    print(f"Input: {data.path}")
    print(f"Events: {len(data.event_ids)}")
    print(f"Photons: {len(data.all_x)}")

    print()
    print("Unordered single-photon x = E/(m_e c^2)")
    print(f"  mean      : {mean(data.all_x):.10f}")
    print(f"  std       : {sample_std(data.all_x):.10f}")
    print(f"  minimum   : {min(data.all_x):.10f}")
    print(f"  q05       : {quantile(data.all_x, 0.05):.10f}")
    print(f"  median    : {quantile(data.all_x, 0.50):.10f}")
    print(f"  q95       : {quantile(data.all_x, 0.95):.10f}")
    print(f"  maximum   : {max(data.all_x):.10f}")

    print()
    print("Ordered photon energies")
    print(
        "  low mean  : "
        f"{mean(data.ordered_low_x):.10f} "
        f"+/- {standard_error(data.ordered_low_x):.10f}"
    )
    print(
        "  mid mean  : "
        f"{mean(data.ordered_mid_x):.10f} "
        f"+/- {standard_error(data.ordered_mid_x):.10f}"
    )
    print(
        "  high mean : "
        f"{mean(data.ordered_high_x):.10f} "
        f"+/- {standard_error(data.ordered_high_x):.10f}"
    )

    print()
    print("Energy closure")
    print(
        "  mean signed error MeV : "
        f"{mean(data.event_energy_errors_MeV):.12e}"
    )
    print(
        "  mean absolute error   : "
        f"{mean(absolute_errors):.12e}"
    )
    print(
        "  maximum absolute error: "
        f"{max(absolute_errors):.12e}"
    )

    print()
    print("Support")
    print(
        "  expected normalized support: "
        "0 < x <= 1 within CSV precision"
    )
    print(
        "  support violations          : "
        f"{len(support_violations)}"
    )


def print_pairwise_comparisons(
    datasets: Sequence[ModelData],
    bin_edges: Sequence[float],
) -> None:
    if len(datasets) < 2:
        return

    print()
    print("=== Pairwise model comparisons ===")

    for first_index in range(len(datasets)):
        for second_index in range(
            first_index + 1,
            len(datasets),
        ):
            first = datasets[first_index]
            second = datasets[second_index]

            print()
            print(
                f"{first.label} vs {second.label}"
            )

            print(
                "  unordered marginal KS       : "
                f"{empirical_ks_distance(first.all_x, second.all_x):.10f}"
            )

            print(
                "  unordered histogram L1      : "
                f"{histogram_l1_distance(first.all_x, second.all_x, bin_edges):.10f}"
            )

            print(
                "  ordered-low KS              : "
                f"{empirical_ks_distance(first.ordered_low_x, second.ordered_low_x):.10f}"
            )

            print(
                "  ordered-middle KS           : "
                f"{empirical_ks_distance(first.ordered_mid_x, second.ordered_mid_x):.10f}"
            )

            print(
                "  ordered-high KS             : "
                f"{empirical_ks_distance(first.ordered_high_x, second.ordered_high_x):.10f}"
            )


def write_summary_csv(
    datasets: Sequence[ModelData],
    output_path: Path,
) -> None:
    with output_path.open(
        "w",
        newline="",
        encoding="utf-8",
    ) as handle:
        writer = csv.writer(handle)

        writer.writerow(
            [
                "label",
                "input_path",
                "event_count",
                "photon_count",
                "mean_x",
                "std_x",
                "min_x",
                "q05_x",
                "median_x",
                "q95_x",
                "max_x",
                "mean_low_x",
                "mean_mid_x",
                "mean_high_x",
                "se_low_x",
                "se_mid_x",
                "se_high_x",
                "mean_abs_energy_error_MeV",
                "max_abs_energy_error_MeV",
                "support_violation_count",
            ]
        )

        for data in datasets:
            absolute_errors = [
                abs(value)
                for value in data.event_energy_errors_MeV
            ]

            support_violation_count = sum(
                1
                for value in data.all_x
                if (
                    value <= 0.0 or
                    value > 1.0 + NORMALIZED_SUPPORT_TOLERANCE
                )
            )

            writer.writerow(
                [
                    data.label,
                    str(data.path),
                    len(data.event_ids),
                    len(data.all_x),
                    mean(data.all_x),
                    sample_std(data.all_x),
                    min(data.all_x),
                    quantile(data.all_x, 0.05),
                    quantile(data.all_x, 0.50),
                    quantile(data.all_x, 0.95),
                    max(data.all_x),
                    mean(data.ordered_low_x),
                    mean(data.ordered_mid_x),
                    mean(data.ordered_high_x),
                    standard_error(data.ordered_low_x),
                    standard_error(data.ordered_mid_x),
                    standard_error(data.ordered_high_x),
                    mean(absolute_errors),
                    max(absolute_errors),
                    support_violation_count,
                ]
            )


def write_pairwise_csv(
    datasets: Sequence[ModelData],
    bin_edges: Sequence[float],
    output_path: Path,
) -> None:
    with output_path.open(
        "w",
        newline="",
        encoding="utf-8",
    ) as handle:
        writer = csv.writer(handle)

        writer.writerow(
            [
                "model_a",
                "model_b",
                "unordered_marginal_ks",
                "unordered_histogram_l1",
                "ordered_low_ks",
                "ordered_mid_ks",
                "ordered_high_ks",
            ]
        )

        for first_index in range(len(datasets)):
            for second_index in range(
                first_index + 1,
                len(datasets),
            ):
                first = datasets[first_index]
                second = datasets[second_index]

                writer.writerow(
                    [
                        first.label,
                        second.label,
                        empirical_ks_distance(
                            first.all_x,
                            second.all_x,
                        ),
                        histogram_l1_distance(
                            first.all_x,
                            second.all_x,
                            bin_edges,
                        ),
                        empirical_ks_distance(
                            first.ordered_low_x,
                            second.ordered_low_x,
                        ),
                        empirical_ks_distance(
                            first.ordered_mid_x,
                            second.ordered_mid_x,
                        ),
                        empirical_ks_distance(
                            first.ordered_high_x,
                            second.ordered_high_x,
                        ),
                    ]
                )


def create_plots(
    datasets: Sequence[ModelData],
    output_prefix: str,
    bin_count: int,
) -> None:
    if plt is None:
        print()
        print(
            "Matplotlib is unavailable; skipping plots."
        )
        return

    plt.figure()

    for data in datasets:
        plt.hist(
            data.all_x,
            bins=bin_count,
            range=(0.0, 1.0),
            density=True,
            histtype="step",
            linewidth=1.5,
            label=data.label,
        )

    plt.xlabel(r"Normalized photon energy $x=E_\gamma/(m_ec^2)$")
    plt.ylabel("Probability density")
    plt.title("Unordered single-photon energy spectrum")
    plt.legend()
    plt.tight_layout()
    plt.savefig(
        f"{output_prefix}_unordered_spectrum.png",
        dpi=200,
    )
    plt.close()

    plt.figure()

    for data in datasets:
        plt.hist(
            data.ordered_low_x,
            bins=bin_count,
            range=(0.0, 1.0),
            density=True,
            histtype="step",
            linewidth=1.5,
            label=f"{data.label}: low",
        )

        plt.hist(
            data.ordered_mid_x,
            bins=bin_count,
            range=(0.0, 1.0),
            density=True,
            histtype="step",
            linewidth=1.5,
            linestyle="--",
            label=f"{data.label}: middle",
        )

        plt.hist(
            data.ordered_high_x,
            bins=bin_count,
            range=(0.0, 1.0),
            density=True,
            histtype="step",
            linewidth=1.5,
            linestyle=":",
            label=f"{data.label}: high",
        )

    plt.xlabel(r"Ordered normalized energy $x=E_\gamma/(m_ec^2)$")
    plt.ylabel("Probability density")
    plt.title("Ordered three-photon energy distributions")
    plt.legend(fontsize="small")
    plt.tight_layout()
    plt.savefig(
        f"{output_prefix}_ordered_spectra.png",
        dpi=200,
    )
    plt.close()

    plt.figure()

    for data in datasets:
        sorted_values = sorted(data.all_x)

        cumulative = [
            (index + 1) / len(sorted_values)
            for index in range(len(sorted_values))
        ]

        plt.plot(
            sorted_values,
            cumulative,
            label=data.label,
        )

    plt.xlabel(r"Normalized photon energy $x=E_\gamma/(m_ec^2)$")
    plt.ylabel("Empirical cumulative probability")
    plt.title("Single-photon empirical CDF")
    plt.legend()
    plt.tight_layout()
    plt.savefig(
        f"{output_prefix}_unordered_cdf.png",
        dpi=200,
    )
    plt.close()


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Compare three-gamma photon-energy distributions "
            "from PsSource annihilation_gammas.csv files."
        )
    )

    parser.add_argument(
        "--input",
        action="append",
        required=True,
        type=parse_input_spec,
        metavar="LABEL:PATH",
        help=(
            "Labeled annihilation_gammas.csv input. "
            "May be supplied multiple times."
        ),
    )

    parser.add_argument(
        "--bins",
        type=int,
        default=80,
        help="Number of histogram bins. Default: 80.",
    )

    parser.add_argument(
        "--output-prefix",
        default="three_gamma_comparison",
        help=(
            "Prefix for generated CSV and PNG files. "
            "Default: three_gamma_comparison."
        ),
    )

    parser.add_argument(
        "--no-plots",
        action="store_true",
        help="Do not create PNG plots.",
    )

    return parser


def main() -> int:
    parser = build_argument_parser()
    args = parser.parse_args()

    if args.bins <= 0:
        parser.error("--bins must be positive.")

    datasets: list[ModelData] = []

    labels_seen: set[str] = set()

    for label, path in args.input:
        if label in labels_seen:
            parser.error(
                f"Duplicate input label: {label}"
            )

        labels_seen.add(label)

        datasets.append(
            read_three_gamma_csv(
                label=label,
                path=path,
            )
        )

    bin_edges = make_uniform_bin_edges(
        minimum=0.0,
        maximum=1.0,
        bin_count=args.bins,
    )

    print("=== Three-Gamma Model Comparison ===")
    print(
        f"Electron mass used: "
        f"{ELECTRON_MASS_MEV:.8f} MeV"
    )
    print(
        f"Expected event energy: "
        f"{EXPECTED_TOTAL_ENERGY_MEV:.8f} MeV"
    )

    for data in datasets:
        print_model_summary(data)

    print_pairwise_comparisons(
        datasets,
        bin_edges,
    )

    summary_path = Path(
        f"{args.output_prefix}_summary.csv"
    )

    pairwise_path = Path(
        f"{args.output_prefix}_pairwise.csv"
    )

    write_summary_csv(
        datasets,
        summary_path,
    )

    write_pairwise_csv(
        datasets,
        bin_edges,
        pairwise_path,
    )

    if not args.no_plots:
        create_plots(
            datasets,
            args.output_prefix,
            args.bins,
        )

    print()
    print("Generated:")
    print(f"  {summary_path}")
    print(f"  {pairwise_path}")

    if not args.no_plots and plt is not None:
        print(
            f"  {args.output_prefix}_unordered_spectrum.png"
        )
        print(
            f"  {args.output_prefix}_ordered_spectra.png"
        )
        print(
            f"  {args.output_prefix}_unordered_cdf.png"
        )

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (
        FileNotFoundError,
        ValueError,
    ) as exc:
        print(
            f"ERROR: {exc}",
            file=sys.stderr,
        )
        raise SystemExit(1)
