#!/usr/bin/env python3
"""Diff the compiled C++ engine (engine_test) against the Python reference over a
window, proving the firmware math matches pytides (via tide_reference)."""
import datetime as dt
import subprocess
import sys

sys.path.insert(0, "../generator")
from tide_reference import Predictor  # noqa: E402

FT_PER_M = 3.280839895


def main():
    string_path = sys.argv[1] if len(sys.argv) > 1 else "fixtures/8452944.string"
    with open(string_path, encoding="utf-8") as fh:
        s = fh.read().strip()

    # Parse the string into (name, H_m, kappa) for the reference predictor.
    z0 = 0.0
    feet = False
    cons = []
    for tok in s.split("|"):
        if tok in ("", "TIDE1"):
            continue
        if tok.startswith("U="):
            feet = tok[2:] == "ft"
        elif tok.startswith("Z0="):
            z0 = float(tok[3:])
        elif "=" in tok:
            continue
        else:
            name, amp, ph = tok.split(":")
            cons.append([name, float(amp), float(ph)])
    if feet:
        z0 /= FT_PER_M
        for c in cons:
            c[1] /= FT_PER_M
    pred = Predictor([(n, H, k) for n, H, k in cons], z0=z0)

    base = int(dt.datetime(2026, 7, 18, tzinfo=dt.timezone.utc).timestamp())
    count, step = 2000, 400  # ~9 days at 6.6-min spacing
    out = subprocess.run(
        ["./engine_test", s, str(base), str(count), str(step)],
        capture_output=True, text=True, check=True,
    ).stdout.strip().splitlines()

    max_d = 0.0
    for line in out:
        ep, h_cpp = line.split(",")
        ep, h_cpp = int(ep), float(h_cpp)
        h_ref = pred.height(ep)  # t0 defaults to ep (instantaneous), matches C++
        max_d = max(max_d, abs(h_cpp - h_ref))
    print(f"C++ vs Python reference: {len(out)} samples")
    print(f"  max |diff| = {max_d * 1000:.6f} mm")
    if max_d > 1e-4:  # 0.1 mm tolerance (float32 amplitudes in the string)
        print("  FAIL: exceeds 0.1 mm")
        sys.exit(1)
    print("  PASS")


if __name__ == "__main__":
    main()
