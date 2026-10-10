#!/usr/bin/env python3
"""Perft regression test: exact node counts on 6 canonical positions.

Depths are chosen so the whole suite runs in seconds on a Release build.
Counts are the published perft values, cross-checked against python-chess
during development (see docs/05-perft-verification.md).
Exit 0 = all match, 1 = mismatch.
"""
import argparse
import re
import subprocess
import sys

CASES = [
    ("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281),
    ("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862),
    ("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238),
    ("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467),
    ("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 0 1", 3, 62379),
    ("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 1", 3, 89890),
]


def run_perft(engine, fen, depth):
    script = f"position fen {fen}\nperft {depth}\nquit\n"
    p = subprocess.run([engine], input=script, capture_output=True, text=True, timeout=300)
    for line in p.stdout.splitlines():
        m = re.match(r"^nodes (\d+)", line)
        if m:
            return int(m.group(1))
    print(f"no perft output for depth {depth}\n--- stdout ---\n{p.stdout}\n--- stderr ---\n{p.stderr}")
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--engine", required=True)
    args = ap.parse_args()
    failures = 0
    for i, (fen, depth, expected) in enumerate(CASES):
        got = run_perft(args.engine, fen, depth)
        status = "ok" if got == expected else "MISMATCH"
        if got != expected:
            failures += 1
        print(f"[{status}] pos{i} depth {depth}: got {got}, expected {expected}")
    print("RESULT:", "PASS" if failures == 0 else f"FAIL ({failures})")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
