#!/usr/bin/env python3
import json
import math
import os
import statistics
from glob import glob

ROOT = "paper_data_errorbars"


def mean(xs):
    return statistics.mean(xs) if xs else float("nan")


def sd(xs):
    if len(xs) < 2:
        return 0.0
    return statistics.stdev(xs)


def sem(xs):
    if len(xs) < 2:
        return 0.0
    return statistics.stdev(xs) / math.sqrt(len(xs))


def load_validation(path):
    with open(path, "r") as f:
        return json.load(f)


def summarize_condition(validation_files):
    frac2 = []
    frac3 = []
    mean_delay = []
    std_delay = []
    energy_err = []
    momentum_err = []

    for vf in validation_files:
        d = load_validation(vf)
        ms = d["mode_summary"]
        cs = d["closure_summary"]

        frac2.append(ms["annihilation_mode_fractions"].get("2", 0.0))
        frac3.append(ms["annihilation_mode_fractions"].get("3", 0.0))
        mean_delay.append(ms["delay_all"]["mean_ns"])
        std_delay.append(ms["delay_all"]["std_ns"])
        energy_err.append(cs["energy_sum_error_MeV"]["mean_abs"])
        momentum_err.append(cs["momentum_closure_error_MeV_over_c"]["mean_abs"])

    return {
        "nrep": len(validation_files),
        "frac2_mean": mean(frac2),
        "frac2_sd": sd(frac2),
        "frac2_sem": sem(frac2),
        "frac3_mean": mean(frac3),
        "frac3_sd": sd(frac3),
        "frac3_sem": sem(frac3),
        "delay_mean_ns": mean(mean_delay),
        "delay_sd_across_runs_ns": sd(mean_delay),
        "delay_sem_across_runs_ns": sem(mean_delay),
        "within_run_delay_sd_mean_ns": mean(std_delay),
        "energy_err_mean": mean(energy_err),
        "energy_err_sd": sd(energy_err),
        "momentum_err_mean": mean(momentum_err),
        "momentum_err_sd": sd(momentum_err),
    }


def write_tsv(path, header, rows):
    with open(path, "w") as f:
        f.write("\t".join(header) + "\n")
        for row in rows:
            f.write("\t".join(str(x) for x in row) + "\n")


def collect_family(family):
    base = os.path.join(ROOT, family)
    tags = sorted(
        d for d in os.listdir(base)
        if os.path.isdir(os.path.join(base, d))
    )
    out = {}
    for tag in tags:
        vfiles = sorted(glob(os.path.join(base, tag, "rep_*", "validation.json")))
        out[tag] = summarize_condition(vfiles)
    return out


def main():
    native = collect_family("native_fraction_sweep")
    explicit = collect_family("explicit_branch_sweep")
    lifetime = collect_family("lifetime_sweep")

    os.makedirs(os.path.join(ROOT, "summaries"), exist_ok=True)

    header_common = [
        "tag",
        "nrep",
        "frac2_mean",
        "frac2_sd",
        "frac2_sem",
        "frac3_mean",
        "frac3_sd",
        "frac3_sem",
        "delay_mean_ns",
        "delay_sd_across_runs_ns",
        "delay_sem_across_runs_ns",
        "within_run_delay_sd_mean_ns",
        "energy_err_mean",
        "energy_err_sd",
        "momentum_err_mean",
        "momentum_err_sd",
    ]

    native_rows = []
    for tag, s in sorted(native.items()):
        native_rows.append([
            tag, s["nrep"],
            s["frac2_mean"], s["frac2_sd"], s["frac2_sem"],
            s["frac3_mean"], s["frac3_sd"], s["frac3_sem"],
            s["delay_mean_ns"], s["delay_sd_across_runs_ns"], s["delay_sem_across_runs_ns"],
            s["within_run_delay_sd_mean_ns"],
            s["energy_err_mean"], s["energy_err_sd"],
            s["momentum_err_mean"], s["momentum_err_sd"],
        ])

    explicit_rows = []
    for tag, s in sorted(explicit.items()):
        explicit_rows.append([
            tag, s["nrep"],
            s["frac2_mean"], s["frac2_sd"], s["frac2_sem"],
            s["frac3_mean"], s["frac3_sd"], s["frac3_sem"],
            s["delay_mean_ns"], s["delay_sd_across_runs_ns"], s["delay_sem_across_runs_ns"],
            s["within_run_delay_sd_mean_ns"],
            s["energy_err_mean"], s["energy_err_sd"],
            s["momentum_err_mean"], s["momentum_err_sd"],
        ])

    lifetime_rows = []
    for tag, s in sorted(lifetime.items(), key=lambda kv: float(kv[0].split("_")[1])):
        lifetime_rows.append([
            tag, s["nrep"],
            s["frac2_mean"], s["frac2_sd"], s["frac2_sem"],
            s["frac3_mean"], s["frac3_sd"], s["frac3_sem"],
            s["delay_mean_ns"], s["delay_sd_across_runs_ns"], s["delay_sem_across_runs_ns"],
            s["within_run_delay_sd_mean_ns"],
            s["energy_err_mean"], s["energy_err_sd"],
            s["momentum_err_mean"], s["momentum_err_sd"],
        ])

    write_tsv(os.path.join(ROOT, "summaries", "native_fraction_sweep.tsv"), header_common, native_rows)
    write_tsv(os.path.join(ROOT, "summaries", "explicit_branch_sweep.tsv"), header_common, explicit_rows)
    write_tsv(os.path.join(ROOT, "summaries", "lifetime_sweep.tsv"), header_common, lifetime_rows)

    with open(os.path.join(ROOT, "summaries", "all_summaries.json"), "w") as f:
        json.dump(
            {
                "native_fraction_sweep": native,
                "explicit_branch_sweep": explicit,
                "lifetime_sweep": lifetime,
            },
            f,
            indent=2,
        )

    print("Wrote:")
    print(os.path.join(ROOT, "summaries", "native_fraction_sweep.tsv"))
    print(os.path.join(ROOT, "summaries", "explicit_branch_sweep.tsv"))
    print(os.path.join(ROOT, "summaries", "lifetime_sweep.tsv"))
    print(os.path.join(ROOT, "summaries", "all_summaries.json"))


if __name__ == "__main__":
    main()
