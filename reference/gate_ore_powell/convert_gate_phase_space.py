#!/usr/bin/env python3

import argparse
import csv
import json
import math
from collections import Counter, defaultdict
from pathlib import Path

import awkward as ak
import numpy as np
import uproot


ELECTRON_MASS_MEV = 0.51099895
EXPECTED_EVENT_ENERGY_MEV = 2.0 * ELECTRON_MASS_MEV


def parse_arguments():
    parser = argparse.ArgumentParser(
        description=(
            "Convert GATE Extended o-Ps PhaseSpaceActor ROOT output "
            "to the PsSource annihilation_gammas.csv schema."
        )
    )

    parser.add_argument(
        "input_root",
        type=Path,
        help="GATE PhaseSpaceActor ROOT file",
    )

    parser.add_argument(
        "output_csv",
        type=Path,
        help="Output annihilation_gammas.csv",
    )

    parser.add_argument(
        "--summary",
        type=Path,
        default=None,
        help="Optional JSON conversion summary",
    )

    parser.add_argument(
        "--tree",
        default="PhaseSpace",
        help="ROOT tree name; newest cycle is selected automatically",
    )

    parser.add_argument(
        "--energy-tolerance-MeV",
        type=float,
        default=5.0e-6,
        help="Maximum allowed event energy-closure error",
    )

    parser.add_argument(
        "--momentum-tolerance-MeV-over-c",
        type=float,
        default=5.0e-6,
        help="Maximum allowed event momentum-closure error",
    )

    return parser.parse_args()


def select_newest_tree(root_file, base_name):
    candidates = []

    for key in root_file.keys(cycle=True):
        name, separator, cycle_text = key.partition(";")

        if name != base_name:
            continue

        cycle = int(cycle_text) if separator else 0
        candidates.append((cycle, key))

    if not candidates:
        raise RuntimeError(
            f"Could not find ROOT tree named {base_name!r}"
        )

    _, selected_key = max(candidates)
    return selected_key, root_file[selected_key]


def decode_strings(array):
    values = ak.to_list(array)

    decoded = []

    for value in values:
        if isinstance(value, bytes):
            decoded.append(value.decode("utf-8"))
        else:
            decoded.append(str(value))

    return np.asarray(decoded, dtype=str)


def main():
    args = parse_arguments()

    if not args.input_root.is_file():
        raise FileNotFoundError(args.input_root)

    args.output_csv.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    summary_path = (
        args.summary
        if args.summary is not None
        else args.output_csv.with_name(
            args.output_csv.stem + "_conversion_summary.json"
        )
    )

    required_branches = [
        "EventID",
        "TrackID",
        "ParentID",
        "Ekine",
        "X",
        "Y",
        "Z",
        "dX",
        "dY",
        "dZ",
        "ParticleName",
        "CreatorProcess",
    ]

    with uproot.open(args.input_root) as root_file:
        selected_tree_key, tree = select_newest_tree(
            root_file,
            args.tree,
        )

        missing = [
            branch
            for branch in required_branches
            if branch not in tree.keys()
        ]

        if missing:
            raise RuntimeError(
                "Missing required ROOT branches: "
                + ", ".join(missing)
            )

        data = tree.arrays(
            required_branches,
            library="ak",
        )

    event_id = ak.to_numpy(data.EventID)
    track_id = ak.to_numpy(data.TrackID)
    parent_id = ak.to_numpy(data.ParentID)
    energy = ak.to_numpy(data.Ekine)

    x = ak.to_numpy(data.X)
    y = ak.to_numpy(data.Y)
    z = ak.to_numpy(data.Z)

    dx = ak.to_numpy(data.dX)
    dy = ak.to_numpy(data.dY)
    dz = ak.to_numpy(data.dZ)

    particle_name = decode_strings(data.ParticleName)
    creator_process = decode_strings(data.CreatorProcess)

    radial_dot = x * dx + y * dy + z * dz

    primary_gamma_mask = (
        (particle_name == "gamma")
        & (parent_id == 0)
        & (radial_dot > 0.0)
    )

    selected_indices = np.flatnonzero(primary_gamma_mask)

    events = defaultdict(list)

    for index in selected_indices:
        events[int(event_id[index])].append(int(index))

    multiplicity_distribution = Counter(
        len(indices)
        for indices in events.values()
    )

    invalid_events = {
        source_event_id: len(indices)
        for source_event_id, indices in events.items()
        if len(indices) != 3
    }

    if invalid_events:
        examples = list(sorted(invalid_events.items()))[:20]

        raise RuntimeError(
            "Filtered GATE events did not all contain exactly "
            f"three primary gammas. Examples: {examples}"
        )

    output_rows = []

    maximum_energy_sum_error = 0.0
    maximum_momentum_error = 0.0
    maximum_direction_norm_error = 0.0

    ordered_source_event_ids = sorted(events)

    for output_event_id, source_event_id in enumerate(
        ordered_source_event_ids
    ):
        indices = sorted(
            events[source_event_id],
            key=lambda index: int(track_id[index]),
        )

        event_energy = float(
            sum(float(energy[index]) for index in indices)
        )

        px = float(
            sum(
                float(energy[index]) * float(dx[index])
                for index in indices
            )
        )
        py = float(
            sum(
                float(energy[index]) * float(dy[index])
                for index in indices
            )
        )
        pz = float(
            sum(
                float(energy[index]) * float(dz[index])
                for index in indices
            )
        )

        energy_error = abs(
            event_energy - EXPECTED_EVENT_ENERGY_MEV
        )

        momentum_error = math.sqrt(
            px * px + py * py + pz * pz
        )

        maximum_energy_sum_error = max(
            maximum_energy_sum_error,
            energy_error,
        )

        maximum_momentum_error = max(
            maximum_momentum_error,
            momentum_error,
        )

        for index in indices:
            direction_norm = math.sqrt(
                float(dx[index]) ** 2
                + float(dy[index]) ** 2
                + float(dz[index]) ** 2
            )

            maximum_direction_norm_error = max(
                maximum_direction_norm_error,
                abs(direction_norm - 1.0),
            )

            process = creator_process[index].strip()

            if not process:
                process = "GATE_Extended_oPs"

            output_rows.append(
                {
                    "event_id": output_event_id,
                    "source_event_id": source_event_id,
                    "track_id": int(track_id[index]),
                    "parent_id": int(parent_id[index]),
                    "creator_process": process,
                    "vertex_time_ns": 0,
                    "vertex_x_mm": 0,
                    "vertex_y_mm": 0,
                    "vertex_z_mm": 0,
                    "kinetic_energy_MeV": format(
                        float(energy[index]),
                        ".17g",
                    ),
                    "dir_x": format(float(dx[index]), ".17g"),
                    "dir_y": format(float(dy[index]), ".17g"),
                    "dir_z": format(float(dz[index]), ".17g"),
                    "pol_x": 0,
                    "pol_y": 0,
                    "pol_z": 0,
                    "polarization_valid": 0,
                }
            )

    failures = []

    if maximum_energy_sum_error > args.energy_tolerance_MeV:
        failures.append(
            "Maximum event energy-closure error exceeded "
            f"{args.energy_tolerance_MeV:.6e} MeV"
        )

    if (
        maximum_momentum_error
        > args.momentum_tolerance_MeV_over_c
    ):
        failures.append(
            "Maximum event momentum-closure error exceeded "
            f"{args.momentum_tolerance_MeV_over_c:.6e} MeV/c"
        )

    fieldnames = [
        "event_id",
        "source_event_id",
        "track_id",
        "parent_id",
        "creator_process",
        "vertex_time_ns",
        "vertex_x_mm",
        "vertex_y_mm",
        "vertex_z_mm",
        "kinetic_energy_MeV",
        "dir_x",
        "dir_y",
        "dir_z",
        "pol_x",
        "pol_y",
        "pol_z",
        "polarization_valid",
    ]

    with args.output_csv.open(
        "w",
        newline="",
        encoding="utf-8",
    ) as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=fieldnames,
        )
        writer.writeheader()
        writer.writerows(output_rows)

    summary = {
        "status": "PASS" if not failures else "FAIL",
        "input_root": str(args.input_root),
        "selected_tree": selected_tree_key,
        "output_csv": str(args.output_csv),
        "raw_phase_space_records": int(len(event_id)),
        "filtered_primary_gamma_records": int(
            len(selected_indices)
        ),
        "events_written": int(len(events)),
        "photons_written": int(len(output_rows)),
        "multiplicity_distribution": {
            str(key): int(value)
            for key, value in sorted(
                multiplicity_distribution.items()
            )
        },
        "maximum_energy_sum_error_MeV": (
            maximum_energy_sum_error
        ),
        "maximum_momentum_error_MeV_over_c": (
            maximum_momentum_error
        ),
        "maximum_direction_norm_error": (
            maximum_direction_norm_error
        ),
        "energy_tolerance_MeV": (
            args.energy_tolerance_MeV
        ),
        "momentum_tolerance_MeV_over_c": (
            args.momentum_tolerance_MeV_over_c
        ),
        "filters": {
            "particle_name": "gamma",
            "parent_id": 0,
            "radial_dot_direction": "> 0",
        },
        "failures": failures,
    }

    with summary_path.open(
        "w",
        encoding="utf-8",
    ) as handle:
        json.dump(
            summary,
            handle,
            indent=2,
            sort_keys=True,
        )
        handle.write("\n")

    print("=== GATE Phase-Space Conversion ===")
    print(f"Input ROOT             : {args.input_root}")
    print(f"Selected tree          : {selected_tree_key}")
    print(f"Raw records            : {len(event_id)}")
    print(f"Primary gamma records  : {len(selected_indices)}")
    print(f"Events written         : {len(events)}")
    print(f"Photons written        : {len(output_rows)}")
    print(
        "Maximum energy error  : "
        f"{maximum_energy_sum_error:.6e} MeV"
    )
    print(
        "Maximum momentum error: "
        f"{maximum_momentum_error:.6e} MeV/c"
    )
    print(f"Output CSV             : {args.output_csv}")
    print(f"Summary JSON           : {summary_path}")
    print(summary["status"])

    if failures:
        for failure in failures:
            print(f"  - {failure}")

        raise SystemExit(1)


if __name__ == "__main__":
    main()
