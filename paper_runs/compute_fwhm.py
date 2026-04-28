#!/usr/bin/env python3
import json
import numpy as np
from pathlib import Path

def fwhm_from_profile(x, y):
    y = np.asarray(y, dtype=float)
    x = np.asarray(x, dtype=float)
    ymax = y.max()
    if ymax <= 0:
        return np.nan
    hm = 0.5 * ymax
    idx = np.where(y >= hm)[0]
    if len(idx) < 2:
        return np.nan
    i1, i2 = idx[0], idx[-1]

    def interp_cross(i_left, i_right):
        x1, x2 = x[i_left], x[i_right]
        y1, y2 = y[i_left], y[i_right]
        if y2 == y1:
            return 0.5 * (x1 + x2)
        return x1 + (hm - y1) * (x2 - x1) / (y2 - y1)

    xl = interp_cross(max(i1 - 1, 0), i1)
    xr = interp_cross(i2, min(i2 + 1, len(x) - 1))
    return xr - xl

def main():
    px = np.loadtxt("profile_x.csv", delimiter=",", skiprows=1)
    py = np.loadtxt("profile_y.csv", delimiter=",", skiprows=1)

    fx = fwhm_from_profile(px[:,0], px[:,1])
    fy = fwhm_from_profile(py[:,0], py[:,1])

    out = {
        "fwhm_x_mm": float(fx),
        "fwhm_y_mm": float(fy),
        "fwhm_mean_mm": float(np.nanmean([fx, fy]))
    }

    with open("fwhm.json", "w") as f:
        json.dump(out, f, indent=2)

if __name__ == "__main__":
    main()
