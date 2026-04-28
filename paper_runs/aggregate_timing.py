#!/usr/bin/env python3
from pathlib import Path
import statistics
import csv

ROOT = Path("timing_matrix")
OUTDIR = Path("paper_tables")
OUTDIR.mkdir(exist_ok=True)

rows = []

for cond_dir in sorted(ROOT.iterdir()):
    if not cond_dir.is_dir():
        continue
    condition = cond_dir.name

    for nev_dir in sorted(cond_dir.iterdir()):
        if not nev_dir.is_dir():
            continue
        n_events = int(nev_dir.name.split("_")[1])

        real_vals, user_vals, sys_vals = [], [], []

        for rep_dir in sorted(nev_dir.iterdir()):
            tfile = rep_dir / "time.txt"
            if not tfile.exists():
                continue

            vals = {}
            with open(tfile) as f:
                for line in f:
                    k, v = line.strip().split()
                    vals[k] = float(v)

            real_vals.append(vals.get("real", float("nan")))
            user_vals.append(vals.get("user", float("nan")))
            sys_vals.append(vals.get("sys", float("nan")))

        if not real_vals:
            continue

        rows.append({
            "condition": condition,
            "n_events": n_events,
            "nrep": len(real_vals),
            "wall_mean_s": statistics.mean(real_vals),
            "wall_sd_s": statistics.stdev(real_vals) if len(real_vals) > 1 else 0.0,
            "wall_per_event_us": 1e6 * statistics.mean(real_vals) / n_events,
            "user_mean_s": statistics.mean(user_vals),
            "sys_mean_s": statistics.mean(sys_vals),
        })

if rows:
    with open(OUTDIR / "timing_summary.tsv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote {OUTDIR / 'timing_summary.tsv'}")
else:
    print("No timing rows found.")
