#!/usr/bin/env python3
import json
import csv
import math
import statistics
from pathlib import Path

ROOT = Path("pointsource_replicates")
OUTDIR = Path("paper_tables")
OUTDIR.mkdir(exist_ok=True)

rows = []

def load_json_allow_nan(path: Path):
    text = path.read_text()

    # normalize common invalid JSON numeric tokens
    text = text.replace(": nan", ": NaN")
    text = text.replace(": -nan", ": NaN")
    text = text.replace(": inf", ": Infinity")
    text = text.replace(": -inf", ": -Infinity")

    return json.loads(
        text,
        parse_constant=lambda x: {
            "NaN": float("nan"),
            "Infinity": float("inf"),
            "-Infinity": float("-inf"),
        }[x]
    )

def finite_only(xs):
    return [x for x in xs if isinstance(x, (int, float)) and math.isfinite(x)]

def mean_or_nan(xs):
    xs = finite_only(xs)
    return statistics.mean(xs) if xs else float("nan")

def sd_or_zero(xs):
    xs = finite_only(xs)
    return statistics.stdev(xs) if len(xs) > 1 else 0.0

for source_dir in sorted(ROOT.iterdir()):
    if not source_dir.is_dir():
        continue
    source = source_dir.name

    for method_dir in sorted(source_dir.iterdir()):
        if not method_dir.is_dir():
            continue
        method = method_dir.name

        fx, fy, fm = [], [], []
        total_events = []
        mode2_events = []
        mode3_events = []
        detected_events = []
        detected_mode2 = []
        detected_mode3 = []
        recon_events = []
        recon_mode2 = []
        recon_mode3 = []

        n_fwhm_finite = 0
        n_fwhm_total = 0

        for rep_dir in sorted(method_dir.iterdir()):
            if not rep_dir.is_dir():
                continue

            fwhm_file = rep_dir / "fwhm.json"
            summary_file = rep_dir / "recon_summary.json"

            if not summary_file.exists():
                continue

            summ = load_json_allow_nan(summary_file)

            total_events.append(summ["total_events"])
            mode2_events.append(summ["mode2_events"])
            mode3_events.append(summ["mode3_events"])
            detected_events.append(summ["detected_events"])
            detected_mode2.append(summ["detected_mode2_events"])
            detected_mode3.append(summ["detected_mode3_events"])
            recon_events.append(summ["reconstructed_events"])
            recon_mode2.append(summ["reconstructed_mode2_events"])
            recon_mode3.append(summ["reconstructed_mode3_events"])

            if fwhm_file.exists():
                fwhm = load_json_allow_nan(fwhm_file)
                n_fwhm_total += 1

                fx_val = fwhm.get("fwhm_x_mm", float("nan"))
                fy_val = fwhm.get("fwhm_y_mm", float("nan"))
                fm_val = fwhm.get("fwhm_mean_mm", float("nan"))

                fx.append(fx_val)
                fy.append(fy_val)
                fm.append(fm_val)

                if all(math.isfinite(v) for v in [fx_val, fy_val, fm_val]):
                    n_fwhm_finite += 1

        if not total_events:
            continue

        rows.append({
            "source_condition": source,
            "method": method,
            "nrep": len(total_events),

            "n_fwhm_total": n_fwhm_total,
            "n_fwhm_finite": n_fwhm_finite,

            "fwhm_x_mean_mm": mean_or_nan(fx),
            "fwhm_x_sd_mm": sd_or_zero(fx),
            "fwhm_y_mean_mm": mean_or_nan(fy),
            "fwhm_y_sd_mm": sd_or_zero(fy),
            "fwhm_mean_mm": mean_or_nan(fm),
            "fwhm_mean_sd_mm": sd_or_zero(fm),

            "total_events_mean": statistics.mean(total_events),
            "mode2_events_mean": statistics.mean(mode2_events),
            "mode3_events_mean": statistics.mean(mode3_events),

            "detected_events_mean": statistics.mean(detected_events),
            "detected_mode2_mean": statistics.mean(detected_mode2),
            "detected_mode3_mean": statistics.mean(detected_mode3),

            "reconstructed_events_mean": statistics.mean(recon_events),
            "reconstructed_events_sd": statistics.stdev(recon_events) if len(recon_events) > 1 else 0.0,
            "reconstructed_mode2_mean": statistics.mean(recon_mode2),
            "reconstructed_mode2_sd": statistics.stdev(recon_mode2) if len(recon_mode2) > 1 else 0.0,
            "reconstructed_mode3_mean": statistics.mean(recon_mode3),
            "reconstructed_mode3_sd": statistics.stdev(recon_mode3) if len(recon_mode3) > 1 else 0.0,
        })

if rows:
    with open(OUTDIR / "recon_fwhm_summary.tsv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)

    with open(OUTDIR / "recon_fwhm_summary.json", "w") as f:
        json.dump(rows, f, indent=2, allow_nan=True)

    print(f"Wrote {OUTDIR / 'recon_fwhm_summary.tsv'}")
    print(f"Wrote {OUTDIR / 'recon_fwhm_summary.json'}")
else:
    print("No rows found.")
