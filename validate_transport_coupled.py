#!/usr/bin/env python3
"""
Validate deterministic transport-coupled PsSource output.

The first implemented case is:

    direct-only, zero-delay, deterministic 2-gamma transport baseline

This validates:
- one realized annihilation per requested event;
- exactly two annihilation photons per event;
- transported-positron parentage;
- annihilation-process provenance;
- terminal-position propagation;
- terminal-time propagation;
- zero sampled Ps delay;
- 511-keV back-to-back photon kinematics;
- unpolarized photon semantics;
- PsSource model provenance.

Example:
    python validate_transport_coupled.py \
        regression_runs/transport_coupled_probe/annihilation_summary.csv \
        regression_runs/transport_coupled_probe/annihilation_gammas.csv \
        --case direct-2g-zero-delay \
        --expected-events 10 \
        --json-out regression_runs/transport_coupled_probe/validation.json
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


ELECTRON_MASS_MEV = 0.510999

SUMMARY_REQUIRED_COLUMNS = {
    "event_id",
    "source_event_id",
    "has_prompt_gamma",
    "annihilation_found",
    "annihilation_mode",
    "n_annihilation_gammas",
    "annihilation_time_ns",
    "annihilation_x_mm",
    "annihilation_y_mm",
    "annihilation_z_mm",
    "positron_range_mm",
    "physics_model_name",
    "physics_model_version",
    "physics_validation_status",
    "positron_terminal_time_ns",
    "sampled_ps_delay_ns",
}

GAMMA_REQUIRED_COLUMNS = {
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
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate transport-coupled PsSource output."
    )

    parser.add_argument(
        "summary",
        type=Path,
        help="Path to annihilation_summary.csv.",
    )

    parser.add_argument(
        "gammas",
        type=Path,
        help="Path to annihilation_gammas.csv.",
    )

    parser.add_argument(
        "--case",
        choices=[
            "direct-2g-zero-delay",
            "pps-2g-fixed-delay",
        ],
        required=True,
        help="Transport regression case to validate.",
    )

    parser.add_argument(
        "--expected-events",
        type=int,
        required=True,
        help="Expected number of annihilation events.",
    )

    parser.add_argument(
        "--json-out",
        type=Path,
        default=None,
        help="Optional JSON summary output.",
    )

    return parser.parse_args()


def require_columns(
    path: Path,
    fieldnames: list[str] | None,
    required: set[str],
) -> None:
    columns = set(fieldnames or [])
    missing = required - columns

    if missing:
        raise ValueError(
            f"{path}: missing required columns: {sorted(missing)}"
        )


def read_summary(path: Path) -> dict[int, dict[str, Any]]:
    if not path.is_file():
        raise FileNotFoundError(f"Input file not found: {path}")

    rows: dict[int, dict[str, Any]] = {}

    with path.open("r", newline="") as handle:
        reader = csv.DictReader(handle)
        require_columns(
            path,
            reader.fieldnames,
            SUMMARY_REQUIRED_COLUMNS,
        )

        for line_number, row in enumerate(reader, start=2):
            event_id = int(row["event_id"])

            if event_id in rows:
                raise ValueError(
                    f"{path}:{line_number}: duplicate event_id {event_id}"
                )

            rows[event_id] = {
                "event_id": event_id,
                "source_event_id": int(row["source_event_id"]),
                "has_prompt_gamma": int(row["has_prompt_gamma"]),
                "annihilation_found": int(row["annihilation_found"]),
                "annihilation_mode": int(row["annihilation_mode"]),
                "n_annihilation_gammas": int(
                    row["n_annihilation_gammas"]
                ),
                "annihilation_time_ns": float(
                    row["annihilation_time_ns"]
                ),
                "position_mm": (
                    float(row["annihilation_x_mm"]),
                    float(row["annihilation_y_mm"]),
                    float(row["annihilation_z_mm"]),
                ),
                "positron_range_mm": float(row["positron_range_mm"]),
                "physics_model_name": row["physics_model_name"],
                "physics_model_version": row["physics_model_version"],
                "physics_validation_status": row[
                    "physics_validation_status"
                ],
                "positron_terminal_time_ns": float(
                    row["positron_terminal_time_ns"]
                ),
                "sampled_ps_delay_ns": float(
                    row["sampled_ps_delay_ns"]
                ),
            }

    if not rows:
        raise ValueError(f"{path}: no event rows found")

    return rows


def read_gammas(
    path: Path,
) -> dict[int, list[dict[str, Any]]]:
    if not path.is_file():
        raise FileNotFoundError(f"Input file not found: {path}")

    grouped: dict[int, list[dict[str, Any]]] = defaultdict(list)

    with path.open("r", newline="") as handle:
        reader = csv.DictReader(handle)
        require_columns(
            path,
            reader.fieldnames,
            GAMMA_REQUIRED_COLUMNS,
        )

        for row in reader:
            event_id = int(row["event_id"])

            grouped[event_id].append(
                {
                    "source_event_id": int(row["source_event_id"]),
                    "track_id": int(row["track_id"]),
                    "parent_id": int(row["parent_id"]),
                    "creator_process": row["creator_process"],
                    "vertex_time_ns": float(row["vertex_time_ns"]),
                    "position_mm": (
                        float(row["vertex_x_mm"]),
                        float(row["vertex_y_mm"]),
                        float(row["vertex_z_mm"]),
                    ),
                    "kinetic_energy_MeV": float(
                        row["kinetic_energy_MeV"]
                    ),
                    "direction": (
                        float(row["dir_x"]),
                        float(row["dir_y"]),
                        float(row["dir_z"]),
                    ),
                    "polarization": (
                        float(row["pol_x"]),
                        float(row["pol_y"]),
                        float(row["pol_z"]),
                    ),
                    "polarization_valid": int(
                        row["polarization_valid"]
                    ),
                }
            )

    if not grouped:
        raise ValueError(f"{path}: no photon rows found")

    return dict(grouped)


def norm(vector: tuple[float, float, float]) -> float:
    return math.sqrt(sum(component * component for component in vector))


def vector_sum(
    first: tuple[float, float, float],
    second: tuple[float, float, float],
) -> tuple[float, float, float]:
    return tuple(
        first[index] + second[index]
        for index in range(3)
    )


def position_difference(
    first: tuple[float, float, float],
    second: tuple[float, float, float],
) -> float:
    return norm(
        tuple(
            first[index] - second[index]
            for index in range(3)
        )
    )


def validate_deterministic_2g(
    summary_rows: dict[int, dict[str, Any]],
    gamma_groups: dict[int, list[dict[str, Any]]],
    expected_events: int,
    case_name: str,
    expected_ps_delay_ns: float,
) -> dict[str, Any]:
    failures: list[str] = []

    summary_tolerance = 1.0e-6
    gamma_tolerance = 1.0e-9
    energy_tolerance_mev = 1.0e-9
    direction_tolerance = 1.0e-9

    if len(summary_rows) != expected_events:
        failures.append(
            f"Expected {expected_events} summary events, "
            f"found {len(summary_rows)}"
        )

    if set(summary_rows) != set(gamma_groups):
        missing_gamma_events = sorted(
            set(summary_rows) - set(gamma_groups)
        )
        extra_gamma_events = sorted(
            set(gamma_groups) - set(summary_rows)
        )

        if missing_gamma_events:
            failures.append(
                "Summary events missing photon rows: "
                f"{missing_gamma_events}"
            )

        if extra_gamma_events:
            failures.append(
                "Photon events missing summary rows: "
                f"{extra_gamma_events}"
            )

    maximum_energy_error = 0.0
    maximum_direction_norm_error = 0.0
    maximum_back_to_back_residual = 0.0
    maximum_gamma_time_difference = 0.0
    maximum_gamma_position_difference = 0.0
    maximum_summary_time_difference = 0.0
    maximum_summary_position_difference = 0.0

    total_gamma_rows = 0

    for event_id, summary in sorted(summary_rows.items()):
        gammas = gamma_groups.get(event_id, [])
        total_gamma_rows += len(gammas)

        if summary["annihilation_found"] != 1:
            failures.append(
                f"Event {event_id}: annihilation_found != 1"
            )

        if summary["annihilation_mode"] != 2:
            failures.append(
                f"Event {event_id}: annihilation_mode != 2"
            )

        if summary["n_annihilation_gammas"] != 2:
            failures.append(
                f"Event {event_id}: n_annihilation_gammas != 2"
            )

        if summary["has_prompt_gamma"] != 0:
            failures.append(
                f"Event {event_id}: unexpected prompt gamma"
            )

        ps_delay_error = abs(
            summary["sampled_ps_delay_ns"]
            - expected_ps_delay_ns
        )

        if ps_delay_error > summary_tolerance:
            failures.append(
                f"Event {event_id}: sampled Ps delay "
                f"{summary['sampled_ps_delay_ns']} ns does not match "
                f"expected {expected_ps_delay_ns} ns"
            )

        expected_summary_time = (
            summary["positron_terminal_time_ns"]
            + summary["sampled_ps_delay_ns"]
        )

        summary_time_difference = abs(
            summary["annihilation_time_ns"]
            - expected_summary_time
        )

        maximum_summary_time_difference = max(
            maximum_summary_time_difference,
            summary_time_difference,
        )

        if summary_time_difference > summary_tolerance:
            failures.append(
                f"Event {event_id}: summary timing decomposition failed"
            )

        if summary["positron_terminal_time_ns"] <= 0.0:
            failures.append(
                f"Event {event_id}: terminal time is not positive"
            )

        if summary["positron_range_mm"] <= 0.0:
            failures.append(
                f"Event {event_id}: positron displacement is not positive"
            )

        expected_provenance = (
            "ConfigurablePsModel/ApproximatePhaseSpace",
            "1.0",
            "approximate-controlled-source-model",
        )

        actual_provenance = (
            summary["physics_model_name"],
            summary["physics_model_version"],
            summary["physics_validation_status"],
        )

        if actual_provenance != expected_provenance:
            failures.append(
                f"Event {event_id}: unexpected provenance "
                f"{actual_provenance}"
            )

        if len(gammas) != 2:
            failures.append(
                f"Event {event_id}: expected 2 photon rows, "
                f"found {len(gammas)}"
            )
            continue

        first, second = gammas

        if first["source_event_id"] != summary["source_event_id"]:
            failures.append(
                f"Event {event_id}: first photon source_event_id mismatch"
            )

        if second["source_event_id"] != summary["source_event_id"]:
            failures.append(
                f"Event {event_id}: second photon source_event_id mismatch"
            )

        if first["creator_process"] != "annihil":
            failures.append(
                f"Event {event_id}: first creator process is not annihil"
            )

        if second["creator_process"] != "annihil":
            failures.append(
                f"Event {event_id}: second creator process is not annihil"
            )

        if first["parent_id"] <= 0:
            failures.append(
                f"Event {event_id}: first parent_id is not positive"
            )

        if second["parent_id"] != first["parent_id"]:
            failures.append(
                f"Event {event_id}: photon parent IDs disagree"
            )

        if first["track_id"] == second["track_id"]:
            failures.append(
                f"Event {event_id}: duplicate photon track IDs"
            )

        gamma_time_difference = abs(
            first["vertex_time_ns"]
            - second["vertex_time_ns"]
        )

        maximum_gamma_time_difference = max(
            maximum_gamma_time_difference,
            gamma_time_difference,
        )

        if gamma_time_difference > gamma_tolerance:
            failures.append(
                f"Event {event_id}: photon birth times disagree"
            )

        summary_gamma_time_difference = abs(
            first["vertex_time_ns"]
            - summary["annihilation_time_ns"]
        )

        maximum_summary_time_difference = max(
            maximum_summary_time_difference,
            summary_gamma_time_difference,
        )

        if summary_gamma_time_difference > summary_tolerance:
            failures.append(
                f"Event {event_id}: photon and summary times disagree"
            )

        gamma_position_difference = position_difference(
            first["position_mm"],
            second["position_mm"],
        )

        maximum_gamma_position_difference = max(
            maximum_gamma_position_difference,
            gamma_position_difference,
        )

        if gamma_position_difference > gamma_tolerance:
            failures.append(
                f"Event {event_id}: photon birth positions disagree"
            )

        summary_position_difference = position_difference(
            first["position_mm"],
            summary["position_mm"],
        )

        maximum_summary_position_difference = max(
            maximum_summary_position_difference,
            summary_position_difference,
        )

        if summary_position_difference > summary_tolerance:
            failures.append(
                f"Event {event_id}: photon and summary positions disagree"
            )

        for index, gamma in enumerate(gammas, start=1):
            energy_error = abs(
                gamma["kinetic_energy_MeV"]
                - ELECTRON_MASS_MEV
            )

            maximum_energy_error = max(
                maximum_energy_error,
                energy_error,
            )

            if energy_error > energy_tolerance_mev:
                failures.append(
                    f"Event {event_id}: photon {index} energy mismatch"
                )

            direction_norm_error = abs(
                norm(gamma["direction"]) - 1.0
            )

            maximum_direction_norm_error = max(
                maximum_direction_norm_error,
                direction_norm_error,
            )

            if direction_norm_error > direction_tolerance:
                failures.append(
                    f"Event {event_id}: photon {index} direction "
                    "is not normalized"
                )

            if gamma["polarization_valid"] != 0:
                failures.append(
                    f"Event {event_id}: photon {index} unexpectedly "
                    "has valid polarization"
                )

            if norm(gamma["polarization"]) > gamma_tolerance:
                failures.append(
                    f"Event {event_id}: photon {index} has nonzero "
                    "polarization"
                )

        back_to_back_residual = norm(
            vector_sum(
                first["direction"],
                second["direction"],
            )
        )

        maximum_back_to_back_residual = max(
            maximum_back_to_back_residual,
            back_to_back_residual,
        )

        if back_to_back_residual > direction_tolerance:
            failures.append(
                f"Event {event_id}: photon directions are not back-to-back"
            )

    return {
        "status": "PASS" if not failures else "FAIL",
        "case": case_name,
        "expected_events": expected_events,
        "summary_events": len(summary_rows),
        "gamma_rows": total_gamma_rows,
        "maximum_energy_error_MeV": maximum_energy_error,
        "maximum_direction_norm_error": maximum_direction_norm_error,
        "maximum_back_to_back_residual": maximum_back_to_back_residual,
        "maximum_gamma_time_difference_ns": (
            maximum_gamma_time_difference
        ),
        "maximum_gamma_position_difference_mm": (
            maximum_gamma_position_difference
        ),
        "maximum_summary_time_difference_ns": (
            maximum_summary_time_difference
        ),
        "maximum_summary_position_difference_mm": (
            maximum_summary_position_difference
        ),
        "failure_count": len(failures),
        "failures": failures,
    }


def main() -> int:
    args = parse_args()

    if args.expected_events <= 0:
        raise ValueError("--expected-events must be positive")

    summary_rows = read_summary(args.summary)
    gamma_groups = read_gammas(args.gammas)

    deterministic_2g_cases = {
        "direct-2g-zero-delay": 0.0,
        "pps-2g-fixed-delay": 3.0,
    }

    if args.case in deterministic_2g_cases:
        result = validate_deterministic_2g(
            summary_rows,
            gamma_groups,
            args.expected_events,
            args.case,
            deterministic_2g_cases[args.case],
        )
    else:
        raise ValueError(f"Unsupported validation case: {args.case}")

    if args.json_out is not None:
        args.json_out.parent.mkdir(
            parents=True,
            exist_ok=True,
        )

        with args.json_out.open("w") as handle:
            json.dump(
                result,
                handle,
                indent=2,
                sort_keys=True,
            )
            handle.write("\n")

    print(
        f"{result['status']}: "
        f"{result['case']} "
        f"({result['summary_events']} events, "
        f"{result['gamma_rows']} photons)"
    )

    if result["failures"]:
        for failure in result["failures"]:
            print(f"  - {failure}", file=sys.stderr)

    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(
            f"FAIL: transport-coupled validation error: {error}",
            file=sys.stderr,
        )
        raise SystemExit(1)
