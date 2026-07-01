#!/usr/bin/env python3
"""
validate_positronium_outputs.py

Validate positronium output CSVs produced by ps_timing / related tools.

Computes:
- 2γ / 3γ fractions
- mean delays
- energy-sum error
- momentum-closure error

Usage:
    python validate_positronium_outputs.py
    python validate_positronium_outputs.py --summary annihilation_summary.csv --gammas annihilation_gammas.csv
    python validate_positronium_outputs.py --json-out validation.json
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List, Tuple, Any


EXPECTED_TOTAL_ENERGY_MEV = 2.0 * 0.510999  # 1.021998 MeV


def read_summary(path: Path) -> List[Dict[str, Any]]:
    rows: List[Dict[str, Any]] = []
    with path.open("r", newline="") as f:
        reader = csv.DictReader(f)
        required = {
            "event_id",
            "annihilation_found",
            "annihilation_mode",
            "n_annihilation_gammas",
            "annihilation_time_ns",
        }
        missing = required - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"{path}: missing required columns: {sorted(missing)}")

        for row in reader:
            rows.append(
                {
                    "event_id": int(row["event_id"]),
                    "annihilation_found": int(row["annihilation_found"]),
                    "annihilation_mode": int(row["annihilation_mode"]),
                    "n_annihilation_gammas": int(row["n_annihilation_gammas"]),
                    "annihilation_time_ns": float(row["annihilation_time_ns"]),
                }
            )
    return rows


def read_gammas(path: Path) -> Dict[int, List[Dict[str, Any]]]:
    grouped: Dict[int, List[Dict[str, Any]]] = defaultdict(list)
    with path.open("r", newline="") as f:
        reader = csv.DictReader(f)
        required = {
            "event_id",
            "kinetic_energy_MeV",
            "dir_x",
            "dir_y",
            "dir_z",
        }
        missing = required - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"{path}: missing required columns: {sorted(missing)}")

        for row in reader:
            event_id = int(row["event_id"])
            grouped[event_id].append(
                {
                    "kinetic_energy_MeV": float(row["kinetic_energy_MeV"]),
                    "dir_x": float(row["dir_x"]),
                    "dir_y": float(row["dir_y"]),
                    "dir_z": float(row["dir_z"]),
                }
            )
    return grouped


def safe_mean(values: List[float]) -> float | None:
    return statistics.mean(values) if values else None


def safe_std(values: List[float]) -> float | None:
    if len(values) < 2:
        return 0.0 if values else None
    return statistics.pstdev(values)


def summarize_modes(summary_rows: List[Dict[str, Any]]) -> Dict[str, Any]:
    found = [r for r in summary_rows if r["annihilation_found"] == 1]
    total_found = len(found)

    mode_counts = Counter(r["annihilation_mode"] for r in found)
    n_gamma_counts = Counter(r["n_annihilation_gammas"] for r in found)

    delays_all = [r["annihilation_time_ns"] for r in found]
    delays_by_mode: Dict[int, List[float]] = defaultdict(list)
    for r in found:
        delays_by_mode[r["annihilation_mode"]].append(r["annihilation_time_ns"])

    mode_fractions = {
        str(mode): (count / total_found if total_found else None)
        for mode, count in sorted(mode_counts.items())
    }

    delay_summary_by_mode = {}
    for mode, vals in sorted(delays_by_mode.items()):
        delay_summary_by_mode[str(mode)] = {
            "count": len(vals),
            "mean_ns": safe_mean(vals),
            "std_ns": safe_std(vals),
            "min_ns": min(vals) if vals else None,
            "max_ns": max(vals) if vals else None,
        }

    return {
        "total_events": len(summary_rows),
        "annihilation_found_events": total_found,
        "annihilation_mode_counts": dict(sorted(mode_counts.items())),
        "n_annihilation_gammas_counts": dict(sorted(n_gamma_counts.items())),
        "annihilation_mode_fractions": mode_fractions,
        "delay_all": {
            "count": len(delays_all),
            "mean_ns": safe_mean(delays_all),
            "std_ns": safe_std(delays_all),
            "min_ns": min(delays_all) if delays_all else None,
            "max_ns": max(delays_all) if delays_all else None,
        },
        "delay_by_mode": delay_summary_by_mode,
    }


def compute_energy_momentum_metrics(
    summary_rows: List[Dict[str, Any]],
    gamma_groups: Dict[int, List[Dict[str, Any]]],
    expected_total_energy_mev: float,
) -> Dict[str, Any]:
    found_rows = [r for r in summary_rows if r["annihilation_found"] == 1]

    energy_sum_errors_abs: List[float] = []
    momentum_closure_abs: List[float] = []

    per_mode_energy_errors: Dict[int, List[float]] = defaultdict(list)
    per_mode_momentum_errors: Dict[int, List[float]] = defaultdict(list)

    missing_gamma_events = 0
    multiplicity_mismatch_events = 0

    for row in found_rows:
        event_id = row["event_id"]
        mode = row["annihilation_mode"]
        expected_n = row["n_annihilation_gammas"]

        gammas = gamma_groups.get(event_id, [])
        if not gammas:
            missing_gamma_events += 1
            continue

        if len(gammas) != expected_n:
            multiplicity_mismatch_events += 1

        energy_sum = 0.0
        px = 0.0
        py = 0.0
        pz = 0.0

        for g in gammas:
            e = g["kinetic_energy_MeV"]
            dx = g["dir_x"]
            dy = g["dir_y"]
            dz = g["dir_z"]

            energy_sum += e
            px += e * dx
            py += e * dy
            pz += e * dz

        energy_err = abs(energy_sum - expected_total_energy_mev)
        p_err = math.sqrt(px * px + py * py + pz * pz)

        energy_sum_errors_abs.append(energy_err)
        momentum_closure_abs.append(p_err)

        per_mode_energy_errors[mode].append(energy_err)
        per_mode_momentum_errors[mode].append(p_err)

    def summarize(values: List[float]) -> Dict[str, float | int | None]:
        return {
            "count": len(values),
            "mean_abs": safe_mean(values),
            "std_abs": safe_std(values),
            "max_abs": max(values) if values else None,
        }

    per_mode = {}
    modes = sorted(set(per_mode_energy_errors.keys()) | set(per_mode_momentum_errors.keys()))
    for mode in modes:
        per_mode[str(mode)] = {
            "energy_sum_error_MeV": summarize(per_mode_energy_errors[mode]),
            "momentum_closure_error_MeV_over_c": summarize(per_mode_momentum_errors[mode]),
        }

    return {
        "expected_total_energy_MeV": expected_total_energy_mev,
        "missing_gamma_events": missing_gamma_events,
        "multiplicity_mismatch_events": multiplicity_mismatch_events,
        "energy_sum_error_MeV": summarize(energy_sum_errors_abs),
        "momentum_closure_error_MeV_over_c": summarize(momentum_closure_abs),
        "per_mode": per_mode,
    }


def print_report(report: Dict[str, Any]) -> None:
    mode_summary = report["mode_summary"]
    closure = report["closure_summary"]

    print("=== Positronium Output Validation ===")
    print(f"summary_csv: {report['summary_csv']}")
    print(f"gammas_csv : {report['gammas_csv']}")
    print()

    print("Event counts")
    print(f"  total events              : {mode_summary['total_events']}")
    print(f"  annihilation-found events : {mode_summary['annihilation_found_events']}")
    print()

    print("Annihilation modes")
    for mode, count in mode_summary["annihilation_mode_counts"].items():
        frac = mode_summary["annihilation_mode_fractions"].get(str(mode))
        if frac is None:
            print(f"  mode {mode}: {count}")
        else:
            print(f"  mode {mode}: {count}  ({frac:.6f})")
    print()

    print("n_annihilation_gammas counts")
    for n_gamma, count in mode_summary["n_annihilation_gammas_counts"].items():
        print(f"  n_gamma {n_gamma}: {count}")
    print()

    delay_all = mode_summary["delay_all"]
    print("Delay summary (all annihilation-found events)")
    print(f"  mean_ns : {delay_all['mean_ns']}")
    print(f"  std_ns  : {delay_all['std_ns']}")
    print(f"  min_ns  : {delay_all['min_ns']}")
    print(f"  max_ns  : {delay_all['max_ns']}")
    print()

    print("Delay summary by mode")
    for mode, stats in mode_summary["delay_by_mode"].items():
        print(f"  mode {mode}:")
        print(f"    count   : {stats['count']}")
        print(f"    mean_ns : {stats['mean_ns']}")
        print(f"    std_ns  : {stats['std_ns']}")
        print(f"    min_ns  : {stats['min_ns']}")
        print(f"    max_ns  : {stats['max_ns']}")
    print()

    print("Energy / momentum closure")
    print(f"  expected_total_energy_MeV      : {closure['expected_total_energy_MeV']}")
    print(f"  missing_gamma_events           : {closure['missing_gamma_events']}")
    print(f"  multiplicity_mismatch_events   : {closure['multiplicity_mismatch_events']}")
    print(f"  mean_abs_energy_sum_error_MeV  : {closure['energy_sum_error_MeV']['mean_abs']}")
    print(f"  max_abs_energy_sum_error_MeV   : {closure['energy_sum_error_MeV']['max_abs']}")
    print(f"  mean_abs_momentum_closure_err  : {closure['momentum_closure_error_MeV_over_c']['mean_abs']}")
    print(f"  max_abs_momentum_closure_err   : {closure['momentum_closure_error_MeV_over_c']['max_abs']}")
    print()

    print("Energy / momentum closure by mode")
    for mode, stats in closure["per_mode"].items():
        e = stats["energy_sum_error_MeV"]
        p = stats["momentum_closure_error_MeV_over_c"]
        print(f"  mode {mode}:")
        print(f"    energy_sum_error_MeV:")
        print(f"      count    : {e['count']}")
        print(f"      mean_abs : {e['mean_abs']}")
        print(f"      std_abs  : {e['std_abs']}")
        print(f"      max_abs  : {e['max_abs']}")
        print(f"    momentum_closure_error_MeV_over_c:")
        print(f"      count    : {p['count']}")
        print(f"      mean_abs : {p['mean_abs']}")
        print(f"      std_abs  : {p['std_abs']}")
        print(f"      max_abs  : {p['max_abs']}")
    print()


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate positronium output CSVs.")
    parser.add_argument(
        "--summary",
        type=Path,
        default=Path("annihilation_summary.csv"),
        help="Path to annihilation_summary.csv",
    )
    parser.add_argument(
        "--gammas",
        type=Path,
        default=Path("annihilation_gammas.csv"),
        help="Path to annihilation_gammas.csv",
    )
    parser.add_argument(
        "--expected-total-energy-mev",
        type=float,
        default=EXPECTED_TOTAL_ENERGY_MEV,
        help="Expected total annihilation photon energy per event in MeV",
    )
    parser.add_argument(
        "--json-out",
        type=Path,
        default=None,
        help="Optional JSON output path",
    )
    args = parser.parse_args()

    if not args.summary.exists():
        print(f"ERROR: summary file not found: {args.summary}", file=sys.stderr)
        return 1
    if not args.gammas.exists():
        print(f"ERROR: gamma file not found: {args.gammas}", file=sys.stderr)
        return 1

    summary_rows = read_summary(args.summary)
    gamma_groups = read_gammas(args.gammas)

    mode_summary = summarize_modes(summary_rows)
    closure_summary = compute_energy_momentum_metrics(
        summary_rows,
        gamma_groups,
        args.expected_total_energy_mev,
    )

    report = {
        "summary_csv": str(args.summary),
        "gammas_csv": str(args.gammas),
        "mode_summary": mode_summary,
        "closure_summary": closure_summary,
    }

    print_report(report)

    if args.json_out is not None:
        with args.json_out.open("w") as f:
            json.dump(report, f, indent=2)
        print(f"Wrote JSON report to: {args.json_out}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
