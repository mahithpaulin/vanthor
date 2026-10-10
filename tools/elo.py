#!/usr/bin/env python3
"""Elo difference + 95% CI from W/D/L. Implements project spec section 8.

    S  = (W + D/2) / N
    dE = -400 * log10(1/S - 1)

CI: normal approx on S (SE = sqrt(S*(1-S)/N) — note: conservative for
paired openings; reported as limitation), endpoints transformed to Elo.
"""
import argparse
import math
import sys

Z95 = 1.96


def score(w, d, l):
    n = w + d + l
    return (w + d / 2) / n if n else 0.0


def elo_diff(s):
    if s <= 0 or s >= 1:
        return float("-inf") if s <= 0 else float("inf")
    return -400.0 * math.log10(1.0 / s - 1.0)


def elo_ci(w, d, l, z=Z95):
    n = w + d + l
    if n == 0:
        raise ValueError("no games")
    s = score(w, d, l)
    se = math.sqrt(max(s * (1 - s), 0) / n)
    lo_s = max(s - z * se, 1e-9)
    hi_s = min(s + z * se, 1 - 1e-9)
    return elo_diff(s), elo_diff(lo_s), elo_diff(hi_s), s, se


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wins", type=int, default=0)
    ap.add_argument("--draws", type=int, default=0)
    ap.add_argument("--losses", type=int, default=0)
    ap.add_argument("--self-check", action="store_true")
    a = ap.parse_args()
    if a.self_check:
        # Known: 50% -> 0 Elo; 55% -> ~+35 Elo.
        assert abs(elo_diff(0.5)) < 1e-9, elo_diff(0.5)
        assert abs(elo_diff(0.55) - 34.86) < 0.1, elo_diff(0.55)
        e, lo, hi, s, se = elo_ci(110, 780, 110)
        assert lo < e < hi, (lo, e, hi)
        print(f"self-check PASS: S={s:.3f} dE={e:.1f} 95%CI=[{lo:.1f},{hi:.1f}] (n=1000)")
        return 0
    try:
        e, lo, hi, s, se = elo_ci(a.wins, a.draws, a.losses)
    except ValueError as ex:
        print(f"error: {ex}")
        return 2
    n = a.wins + a.draws + a.losses
    print(f"W={a.wins} D={a.draws} L={a.losses} N={n} S={s:.4f} (SE={se:.4f})")
    print(f"dE={e:.1f} 95%CI=[{lo:.1f},{hi:.1f}]")
    print("note: CI assumes independent games; paired openings narrow true variance — treat as approx.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
