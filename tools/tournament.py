#!/usr/bin/env python3
"""Tournament-runner skeleton (v0.0.1).

--self-test: no engines needed. Writes a manifest + demo PGN + Elo summary
  using tools/elo.py math, proving the reporting pipeline works in CI.
Real matches (cutechess-cli) arrive in v0.4.0; the CLI surface is frozen now
so CI and docs can rely on it:
  tournament.py --config tournaments/primary.json --out tournaments/<id>/
"""
import argparse
import datetime
import hashlib
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def elo_summary(w, d, l):
    r = subprocess.run(
        [sys.executable, os.path.join(HERE, "elo.py"),
         "--wins", str(w), "--draws", str(d), "--losses", str(l)],
        capture_output=True, text=True)
    return r.stdout.strip()


def self_test(outdir):
    os.makedirs(outdir, exist_ok=True)
    # Demo result only — labeled synthetic, never presented as a real match.
    w, d, l = 5, 12, 3
    manifest = {
        "id": "self-test",
        "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "note": "SYNTHETIC pipeline check. Not a real engine match.",
        "engines": [
            {"name": "vanthor-stub", "version": "0.0.1", "sha256": "n/a-stub"},
            {"name": "stockfish19-reference", "version": "sf_19", "sha256": "recorded-at-match-time"},
        ],
        "config": {"tc": "40/120+1", "threads": 2, "hash_mb": 256,
                   "openings": "paired", "adjudication": "see protocol doc"},
        "result": {"wins": w, "draws": d, "losses": l},
        "exclusions": [],
    }
    blob = json.dumps(manifest, sort_keys=True).encode()
    manifest["manifest_sha256"] = hashlib.sha256(blob).hexdigest()
    with open(os.path.join(outdir, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    with open(os.path.join(outdir, "games.pgn"), "w") as f:
        f.write('[Result "1-0"]\n1. e4 e5 2. Qh5 Nc6 3. Bc4 Nf6 4. Qxf7# 1-0\n\n')
    summary = elo_summary(w, d, l)
    with open(os.path.join(outdir, "summary.txt"), "w") as f:
        f.write(summary + "\n")
    print(summary)
    print(f"self-test PASS: wrote {outdir}/manifest.json games.pgn summary.txt")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--out", default="tournaments/self-test")
    ap.add_argument("--config", default="")
    a = ap.parse_args()
    if a.self_test:
        return self_test(a.out)
    print("real matches need cutechess-cli + two UCI engines (v0.4.0). "
          "Use --self-test for the pipeline check.")
    return 2


if __name__ == "__main__":
    sys.exit(main())
