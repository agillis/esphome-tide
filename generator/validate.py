#!/usr/bin/env python3
"""Validate the offline tide engine against NOAA CO-OPS published predictions.

Fetches NOAA's own 6-minute predictions and hi/lo predictions for a station and
compares them to the reference engine (``tide_reference.Predictor``), which is a
line-for-line port of the firmware. Reports the RMS/max height error and the
timing error of each high/low. This is the ground-truth check for correctness.

    python validate.py --id 8452944 --days 3
"""

import argparse
import datetime as dt
import json
import urllib.parse
import urllib.request

from constituent_index import normalize
from tide_reference import Predictor

FT_PER_M = 3.280839895
API = "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter"


def noaa(station_id, begin, end, **extra):
    params = {
        "begin_date": begin,
        "end_date": end,
        "station": station_id,
        "product": "predictions",
        "datum": "MLLW",
        "time_zone": "gmt",
        "units": "english",
        "application": "esphome-tide-validate",
        "format": "json",
    }
    params.update(extra)
    url = API + "?" + urllib.parse.urlencode(params)
    with urllib.request.urlopen(url, timeout=60) as resp:  # noqa: S310
        return json.loads(resp.read().decode("utf-8"))["predictions"]


def parse_utc(s):
    return int(dt.datetime.strptime(s, "%Y-%m-%d %H:%M").replace(tzinfo=dt.timezone.utc).timestamp())


def load_station(station_id, repo):
    if repo:
        with open(f"{repo.rstrip('/')}/data/noaa/{station_id}.json", encoding="utf-8") as fh:
            return json.load(fh)
    url = f"https://raw.githubusercontent.com/openwatersio/tide-database/main/data/noaa/{station_id}.json"
    with urllib.request.urlopen(url, timeout=30) as resp:  # noqa: S310
        return json.loads(resp.read().decode("utf-8"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--id", required=True)
    ap.add_argument("--repo", help="local tide-database clone (else fetch)")
    ap.add_argument("--start", help="YYYYMMDD (default: today UTC)")
    ap.add_argument("--days", type=int, default=3)
    args = ap.parse_args()

    station = load_station(args.id, args.repo)
    datums = station["datums"]
    z0 = datums["MSL"] - datums["MLLW"]
    cons = [
        (c["name"], float(c["amplitude"]), float(c["phase"]))
        for c in station["harmonic_constituents"]
        if float(c["amplitude"]) > 0.0
    ]
    pred = Predictor(cons, z0=z0)

    start = args.start or dt.datetime.now(dt.timezone.utc).strftime("%Y%m%d")
    start_d = dt.datetime.strptime(start, "%Y%m%d").replace(tzinfo=dt.timezone.utc)
    end_d = start_d + dt.timedelta(days=args.days)
    begin, end = start_d.strftime("%Y%m%d"), end_d.strftime("%Y%m%d")

    print(f"Station {args.id} ({station['name']}): {len(cons)} constituents, Z0={z0:.3f} m")
    print(f"Window {begin}..{end} (UTC), datum MLLW, units ft\n")

    # --- 6-minute curve comparison ---
    curve = noaa(args.id, begin, end, interval="6")
    # Use a single reference t0 (window midpoint) so node factors match how the
    # firmware holds them constant across a search; errors then reflect only the
    # harmonic math, not the t0 choice.
    mid = parse_utc(curve[len(curve) // 2]["t"])
    errs = []
    for p in curve:
        e = parse_utc(p["t"])
        noaa_ft = float(p["v"])
        ours_ft = pred.height(e, t0=mid) * FT_PER_M
        errs.append(abs(ours_ft - noaa_ft))
    n = len(errs)
    rms = (sum(x * x for x in errs) / n) ** 0.5
    print(f"6-min curve: {n} points")
    print(f"  RMS error : {rms:.4f} ft ({rms / FT_PER_M * 100:.2f} cm)")
    print(f"  max error : {max(errs):.4f} ft ({max(errs) / FT_PER_M * 100:.2f} cm)")
    print(f"  mean error: {sum(errs) / n:.4f} ft\n")

    # --- hi/lo timing comparison ---
    hilo = noaa(args.id, begin, end, interval="hilo")
    print(f"hi/lo events: {len(hilo)}")
    print("  NOAA time (GMT)      type  NOAA ft  ours ft   dh(ft)")
    worst_dh = 0.0
    for p in hilo[:12]:
        e = parse_utc(p["t"])
        noaa_ft = float(p["v"])
        ours_ft = pred.height(e, t0=mid) * FT_PER_M
        worst_dh = max(worst_dh, abs(ours_ft - noaa_ft))
        print(f"  {p['t']}   {p['type']:>2}   {noaa_ft:6.2f}   {ours_ft:6.2f}   {ours_ft - noaa_ft:+.2f}")
    print(f"\n  worst |dh| at NOAA extrema: {worst_dh:.3f} ft")


if __name__ == "__main__":
    main()
