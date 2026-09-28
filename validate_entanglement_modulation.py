#!/usr/bin/env python3
"""Analyze entanglement_validation output.

For each configuration (native, direct, pickoff, legacy) this script builds
the entanglement ON/OFF delta-phi ratio for photon pairs whose two Compton
scattering angles fall in a window, fits

    R(dphi) = A * [1 - M * cos(2 * dphi)]

by weighted linear least squares, and compares each PsSource configuration
with native Geant4.

It also reports the analytic Pryce-Ward/Snyder expectation for the accepted
ON events, M_theory = < s1 s2 / K >, where s = sin^2(theta) and
K = g1 g2 - g1 s2 - g2 s1 + s1 s2 with g = e + 1/e, e = 2 - cos(theta).

Two metrics are reported: M_onoff (ON/OFF ratio, the QEPET convention) and
M_unpol (ON against the unpolarized legacy sample). Native Geant4 gives
entanglement-OFF photons perpendicular classical polarizations, so M_onoff
is smaller than the full quantum modulation M_unpol.

Expected results (gated):
  direct, pickoff         : M_onoff and M_unpol consistent with native
  legacy                  : M_onoff consistent with zero (no entanglement)
  native                  : M_onoff > 0 (entanglement is visible)
Reported, not gated:
  native M_unpol          : near the free-electron Pryce-Ward expectation

Input layout (as written by run_entanglement_validation_br200.sbatch):

    RESULTS/<mode>_qe<on|off>/*.csv

Exit status is 0 when every check passes and 1 otherwise.
"""

import argparse
import glob
import json
import math
import os
import sys

import numpy as np

MODES = ["native", "direct", "pickoff", "legacy"]


def load_configuration(results_dir, mode, qe):
    files = sorted(
        glob.glob(os.path.join(results_dir, f"{mode}_qe{qe}", "*.csv"))
    )
    arrays = []
    for path in files:
        data = np.loadtxt(
            path, delimiter=",", skiprows=1, usecols=(0, 1, 2), ndmin=2
        )
        if data.size:
            arrays.append(data)
    if not arrays:
        return None, len(files)
    return np.vstack(arrays), len(files)


def select_window(pairs, theta_min, theta_max):
    theta1 = pairs[:, 0]
    theta2 = pairs[:, 1]
    keep = (
        (theta1 >= theta_min) & (theta1 <= theta_max)
        & (theta2 >= theta_min) & (theta2 <= theta_max)
    )
    return pairs[keep]


def fit_modulation(on_dphi_deg, off_dphi_deg, bins):
    """Fit numerator/denominator delta-phi ratio to A[1 - M cos 2dphi]."""
    edges = np.linspace(0.0, 180.0, bins + 1)
    on_counts, _ = np.histogram(on_dphi_deg, bins=edges)
    off_counts, _ = np.histogram(off_dphi_deg, bins=edges)

    if np.any(on_counts == 0) or np.any(off_counts == 0):
        raise ValueError("empty delta-phi bin; increase statistics or reduce --bins")

    on_norm = on_counts / on_counts.sum()
    off_norm = off_counts / off_counts.sum()
    ratio = on_norm / off_norm

    # Poisson errors on both histograms, propagated to the ratio.
    ratio_err = ratio * np.sqrt(1.0 / on_counts + 1.0 / off_counts)

    centers = 0.5 * (edges[:-1] + edges[1:])

    # Bin-averaged cos(2 dphi), so finite bin width does not bias M.
    lo = np.radians(edges[:-1])
    hi = np.radians(edges[1:])
    cos2 = (np.sin(2.0 * hi) - np.sin(2.0 * lo)) / (2.0 * (hi - lo))

    # R = a + b cos(2 dphi) with a = A and b = -A M.
    design = np.column_stack([np.ones_like(cos2), cos2])
    weights = 1.0 / ratio_err**2
    normal = design.T @ (design * weights[:, None])
    covariance = np.linalg.inv(normal)
    a, b = covariance @ (design.T @ (weights * ratio))

    modulation = -b / a
    # Error propagation for M = -b / a.
    var_a = covariance[0, 0]
    var_b = covariance[1, 1]
    cov_ab = covariance[0, 1]
    modulation_err = math.sqrt(
        (b / a**2) ** 2 * var_a + (1.0 / a) ** 2 * var_b
        - 2.0 * (b / a**2) * (1.0 / a) * cov_ab
    )

    residual = ratio - design @ np.array([a, b])
    chi2 = float(np.sum(weights * residual**2))

    return {
        "M": float(modulation),
        "M_err": float(modulation_err),
        "A": float(a),
        "chi2": chi2,
        "ndf": int(bins - 2),
        "centers_deg": centers.tolist(),
        "ratio": ratio.tolist(),
        "ratio_err": ratio_err.tolist(),
    }


def pryce_ward_modulation(pairs):
    theta = np.radians(pairs[:, :2])
    e = 2.0 - np.cos(theta)
    g = e + 1.0 / e
    s = np.sin(theta) ** 2
    g1, g2 = g[:, 0], g[:, 1]
    s1, s2 = s[:, 0], s[:, 1]
    k = g1 * g2 - g1 * s2 - g2 * s1 + s1 * s2
    return float(np.mean(s1 * s2 / k))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("results_dir")
    parser.add_argument("--theta-min", type=float, default=72.0)
    parser.add_argument("--theta-max", type=float, default=92.0)
    parser.add_argument("--bins", type=int, default=18)
    parser.add_argument(
        "--sigma", type=float, default=3.0,
        help="agreement threshold in combined standard errors",
    )
    parser.add_argument("--json", default=None, help="write results to this file")
    parser.add_argument("--plot", default=None, help="write a ratio plot (PNG/PDF)")
    args = parser.parse_args()

    # Two modulation metrics per configuration:
    #   M_onoff : entanglement ON / entanglement OFF (the QEPET convention).
    #             Native Geant4 gives the OFF photons perpendicular classical
    #             polarizations, so the OFF reference is itself modulated and
    #             M_onoff is smaller than the full quantum modulation.
    #   M_unpol : entanglement ON / unpolarized reference. The legacy
    #             configuration emits unpolarized photons, so its pooled ON+OFF
    #             sample is the unpolarized reference. M_unpol should match
    #             the analytic Pryce-Ward expectation.
    windows = {}
    for mode in MODES:
        on, n_on_files = load_configuration(args.results_dir, mode, "on")
        off, n_off_files = load_configuration(args.results_dir, mode, "off")
        if on is None or off is None:
            print(f"{mode:8s}  missing data (on files={n_on_files}, off files={n_off_files})")
            continue
        windows[mode] = (
            select_window(on, args.theta_min, args.theta_max),
            select_window(off, args.theta_min, args.theta_max),
        )

    unpolarized = None
    if "legacy" in windows:
        unpolarized = np.vstack(windows["legacy"])

    results = {}
    for mode, (on_w, off_w) in windows.items():
        fit = fit_modulation(on_w[:, 2], off_w[:, 2], args.bins)
        fit["N_on"] = int(len(on_w))
        fit["N_off"] = int(len(off_w))
        fit["M_theory"] = pryce_ward_modulation(on_w)
        if unpolarized is not None and mode != "legacy":
            unpol = fit_modulation(on_w[:, 2], unpolarized[:, 2], args.bins)
            fit["M_unpol"] = unpol["M"]
            fit["M_unpol_err"] = unpol["M_err"]
        results[mode] = fit

    print()
    print(
        f"Compton window {args.theta_min:g}-{args.theta_max:g} deg, "
        f"{args.bins} delta-phi bins, fit R = A[1 - M cos 2dphi]"
    )
    print(
        f"{'mode':8s} {'N_on':>10s} {'N_off':>10s} "
        f"{'M_onoff':>16s} {'M_unpol':>16s} {'M_theory':>9s} {'chi2/ndf':>10s}"
    )
    for mode, fit in results.items():
        unpol = (
            f"{fit['M_unpol']:7.4f} +/- {fit['M_unpol_err']:.4f}"
            if "M_unpol" in fit else f"{'-':>16s}"
        )
        print(
            f"{mode:8s} {fit['N_on']:10d} {fit['N_off']:10d} "
            f"{fit['M']:7.4f} +/- {fit['M_err']:.4f} {unpol} "
            f"{fit['M_theory']:9.4f} {fit['chi2']:6.1f}/{fit['ndf']}"
        )

    def agreement(name, value, error, reference, reference_error):
        diff = value - reference
        err = math.hypot(error, reference_error)
        ok = abs(diff) <= args.sigma * err
        return (
            name, ok,
            f"dM = {diff:+.4f} +/- {err:.4f} ({abs(diff) / err:.2f} sigma)",
        )

    checks = []
    info = []
    native = results.get("native")

    if native is not None and "M_unpol" in native:
        # Harness sanity check, reported but not gated: the analytic formula
        # assumes free electrons, while Geant4's polarized Compton model
        # includes binding effects, so small differences are expected at
        # high statistics. It is a statement about Geant4, not PsSource.
        info.append(agreement(
            "native M_unpol vs analytic Pryce-Ward (free electron)",
            native["M_unpol"], native["M_unpol_err"], native["M_theory"], 0.0,
        ))

    for mode in ["direct", "pickoff"]:
        fit = results.get(mode)
        if native is None or fit is None:
            checks.append((f"{mode} vs native", False, "missing data"))
            continue
        checks.append(agreement(
            f"{mode} vs native (M_onoff)",
            fit["M"], fit["M_err"], native["M"], native["M_err"],
        ))
        if "M_unpol" in fit and "M_unpol" in native:
            checks.append(agreement(
                f"{mode} vs native (M_unpol)",
                fit["M_unpol"], fit["M_unpol_err"],
                native["M_unpol"], native["M_unpol_err"],
            ))

    legacy = results.get("legacy")
    if legacy is None:
        checks.append(("legacy consistent with M = 0", False, "missing data"))
    else:
        ok = abs(legacy["M"]) <= args.sigma * legacy["M_err"]
        checks.append((
            "legacy (3.0.0 negative control) consistent with M = 0", ok,
            f"M = {legacy['M']:+.4f} +/- {legacy['M_err']:.4f}",
        ))

    if native is not None:
        # Entanglement must actually be visible in the reference.
        ok = native["M"] > args.sigma * native["M_err"]
        checks.append((
            "native shows entanglement (M_onoff > 0)", ok,
            f"M = {native['M']:.4f} +/- {native['M_err']:.4f}",
        ))

    print()
    for name, ok, detail in checks:
        print(f"{'PASS' if ok else 'FAIL'}  {name}: {detail}")
    for name, _, detail in info:
        print(f"INFO  {name}: {detail}")

    all_ok = all(ok for _, ok, _ in checks)

    if args.json:
        with open(args.json, "w") as handle:
            json.dump(
                {
                    "theta_window_deg": [args.theta_min, args.theta_max],
                    "bins": args.bins,
                    "sigma": args.sigma,
                    "results": results,
                    "checks": [
                        {"name": n, "pass": ok, "detail": d} for n, ok, d in checks
                    ],
                    "info": [
                        {"name": n, "within_sigma": ok, "detail": d}
                        for n, ok, d in info
                    ],
                    "all_pass": all_ok,
                },
                handle,
                indent=2,
            )

    plt = None
    if args.plot and results:
        try:
            import matplotlib
            matplotlib.use("Agg")
            import matplotlib.pyplot as plt
        except ImportError:
            print(f"matplotlib not available; skipping plot {args.plot}")

    if plt is not None:

        fig, ax = plt.subplots(figsize=(6.5, 4.5))
        markers = {"native": "o", "direct": "s", "pickoff": "^", "legacy": "x"}
        for mode, fit in results.items():
            centers = np.array(fit["centers_deg"])
            ax.errorbar(
                centers, fit["ratio"], yerr=fit["ratio_err"],
                fmt=markers.get(mode, "o"), ms=4, capsize=2,
                label=f"{mode}: M = {fit['M']:.3f} $\\pm$ {fit['M_err']:.3f}",
            )
            grid = np.linspace(0.0, 180.0, 361)
            ax.plot(
                grid, fit["A"] * (1.0 - fit["M"] * np.cos(2.0 * np.radians(grid))),
                lw=0.8,
            )
        ax.set_xlabel(r"$\Delta\phi$ (deg)")
        ax.set_ylabel("ON / OFF (normalized)")
        ax.set_title(
            f"Compton window {args.theta_min:g}-{args.theta_max:g} deg"
        )
        ax.legend(fontsize=8)
        fig.tight_layout()
        fig.savefig(args.plot, dpi=200)

    print()
    print("ALL CHECKS PASSED" if all_ok else "SOME CHECKS FAILED")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
