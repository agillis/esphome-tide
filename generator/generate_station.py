#!/usr/bin/env python3
"""Generate an offline-tide ``station_data`` string for the ESPHome ``tide``
component from the openwatersio/tide-database.

The output string embeds only the per-station data (datum offset, MHW/MLW, and
each constituent's amplitude + Greenwich phase). Constituent speeds, Doodson
numbers and nodal-correction formulas live in the C++ component, so the string
stays a few hundred bytes.

Examples
--------
    # Fetch station 8452944 straight from GitHub and print the string:
    python generate_station.py --id 8452944

    # Use a local clone and also print an ESPHome substitution snippet:
    python generate_station.py --id 9414290 --repo ~/src/tide-database --yaml

    # Sanity-check: print 24 h of predicted heights (needs no hardware):
    python generate_station.py --id 8452944 --sample 24
"""

import argparse
import json
import sys
import urllib.request

from constituent_index import CANONICAL, normalize

RAW_URL = "https://raw.githubusercontent.com/openwatersio/tide-database/main/data/noaa/{id}.json"
FT_PER_M = 3.280839895


def load_station(station_id, repo=None):
    if repo:
        path = f"{repo.rstrip('/')}/data/noaa/{station_id}.json"
        with open(path, "r", encoding="utf-8") as fh:
            return json.load(fh)
    url = RAW_URL.format(id=station_id)
    with urllib.request.urlopen(url, timeout=30) as resp:  # noqa: S310 (trusted host)
        return json.loads(resp.read().decode("utf-8"))


def build_string(station, datum="MLLW", drop_zero_amp=True):
    datums = station.get("datums", {})
    if "MSL" not in datums or datum not in datums:
        raise ValueError(f"station missing MSL or {datum} datum")
    datum_val = datums[datum]
    z0 = datums["MSL"] - datum_val  # harmonic sum is MSL-referenced

    parts = ["TIDE1", "U=m", f"Z0={z0:.4f}", f"D={datum}"]
    if "MHW" in datums:
        parts.append(f"MHW={datums['MHW'] - datum_val:.4f}")
    if "MLW" in datums:
        parts.append(f"MLW={datums['MLW'] - datum_val:.4f}")

    kept, dropped, unknown = [], 0, []
    for c in station.get("harmonic_constituents", []):
        name = normalize(c["name"])
        amp = float(c["amplitude"])
        phase = float(c["phase"]) % 360.0
        if name not in CANONICAL:
            unknown.append(c["name"])
            continue
        if drop_zero_amp and amp == 0.0:
            dropped += 1
            continue
        kept.append((name, amp, phase))

    if unknown:
        # Fail loudly: a name the firmware doesn't know would be silently zeroed
        # on-device, quietly degrading the prediction.
        raise ValueError(
            "unknown constituent name(s) not in the component table: "
            + ", ".join(sorted(set(unknown)))
            + " (add them to tide_constituents.cpp / constituent_index.py)"
        )

    for name, amp, phase in kept:
        parts.append(f"{name}:{amp:g}:{phase:g}")

    meta = {
        "kept": len(kept),
        "dropped_zero": dropped,
        "z0": z0,
        "datum": datum,
    }
    return "|".join(parts), meta


def sample(station, hours):
    """Print predicted heights (ft, MLLW) at 30-min steps using the reference
    engine — a quick eyeball check that the string is sane."""
    import datetime as dt

    from tide_reference import Predictor

    datums = station["datums"]
    z0 = datums["MSL"] - datums["MLLW"]
    cons = [
        (c["name"], float(c["amplitude"]), float(c["phase"]))
        for c in station["harmonic_constituents"]
        if float(c["amplitude"]) > 0.0
    ]
    pred = Predictor(cons, z0=z0)
    now = int(dt.datetime.now(dt.timezone.utc).timestamp())
    print(f"# predicted height (ft, MLLW) for {station['name']} — {hours} h from now (UTC)")
    for m in range(0, hours * 60 + 1, 30):
        e = now + m * 60
        h_m = pred.height(e)
        ts = dt.datetime.fromtimestamp(e, dt.timezone.utc).strftime("%m-%d %H:%M")
        print(f"{ts}Z  {h_m * FT_PER_M:6.2f}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--id", required=True, help="NOAA station id (openwatersio data/noaa/<id>.json)")
    ap.add_argument("--source", default="openwatersio", choices=["openwatersio"], help="data source")
    ap.add_argument("--repo", help="path to a local tide-database clone (else fetch from GitHub)")
    ap.add_argument("--datum", default="MLLW", help="chart datum for predictions (default MLLW)")
    ap.add_argument("--keep-zero-amp", action="store_true", help="keep zero-amplitude constituents")
    ap.add_argument("--out", help="write the string to this file (default: stdout)")
    ap.add_argument("--yaml", action="store_true", help="also print an ESPHome substitution snippet")
    ap.add_argument("--sample", type=int, metavar="HOURS", help="print predicted heights for N hours and exit")
    args = ap.parse_args()

    station = load_station(args.id, args.repo)

    if args.sample:
        sample(station, args.sample)
        return

    s, meta = build_string(station, datum=args.datum, drop_zero_amp=not args.keep_zero_amp)

    if args.out:
        with open(args.out, "w", encoding="utf-8") as fh:
            fh.write(s + "\n")
        where = args.out
    else:
        where = "stdout"

    print(s)
    print(
        f"# {station.get('name', args.id)} ({args.id}): {meta['kept']} constituents, "
        f"Z0={meta['z0']:.3f} m, datum={meta['datum']}, {len(s)} bytes -> {where}",
        file=sys.stderr,
    )
    if args.yaml:
        print("\n# --- paste into your ESPHome config ---", file=sys.stderr)
        print("substitutions:", file=sys.stderr)
        print(f'  tide_station_data: "{s}"', file=sys.stderr)


if __name__ == "__main__":
    main()
