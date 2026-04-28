#!/usr/bin/env python3
from pathlib import Path
import json, csv, statistics

ROOT = Path("paper_runs/pointsource")
rows = []

for sdir in sorted(ROOT.iterdir()):
    if not sdir.is_dir():
        continue
    source_cond = sdir.name

    for mdir in sorted(sdir.iterdir()):
        if not mdir.is_dir():
            continue
        method = mdir.name

        fx, fy, fm = [], [], []
        for repdir in sorted(mdir.iterdir()):
            f = repdir / "fwhm.json"
            if not f.exists():
                continue
            d = json.load(open(f))
            fx.append(d["fwhm_x_mm"])
            fy.append(d["fwhm_y_mm"])
            fm.append(d["fwhm_mean_mm"])

        if fx:
            rows.append({
                "source_condition": source_cond,
                "method": method,
                "nrep": len(fx),
                "fwhm_x_mean_mm": statistics.mean(fx),
                "fwhm_x_sd_mm": statistics.stdev(fx) if len(fx) > 1 else 0.0,
                "fwhm_y_mean_mm": statistics.mean(fy),
                "fwhm_y_sd_mm": statistics.stdev(fy) if len(fy) > 1 else 0.0,
                "fwhm_mean_mm": statistics.mean(fm),
                "fwhm_mean_sd_mm": statistics.stdev(fm) if len(fm) > 1 else 0.0,
            })

outdir = Path("paper_tables")
outdir.mkdir(exist_ok=True)

with open(outdir / "recon_fwhm_summary.tsv", "w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()), delimiter="\t")
    writer.writeheader()
    writer.writerows(rows)
