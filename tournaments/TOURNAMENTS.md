# Tournament log

## T-0001 — v0.2.0-dev vs v0.1.0 (2026-10-10, Codespaces 2-core box)

- White binary: vanthor @ `119eb9b` (dev, v0.2.0 search)
  sha256 `e3a6e7a699e862222e778198a03ca251532748ec35fd6d1679c639209a58f056`
- Black binary: vanthor @ `c8911de` (tag v0.1.0)
  sha256 `24317d55dd0869d86c0d1335b926e9888983abb9d35dc67c308fdd5df522d734`
- Config: 96 games (24-book × paired colors), movetime 100ms, Threads 1,
  Hash 32. Runner: tools/play_match.py.
- Result: **W92 D4 L0** (engine-A perspective), S=0.9792,
  dE=+668.8, 95% CI=[+513.7, +3600.0] (upper bound saturates near 100%).
- Exclusions: none. One 41,292-byte PGN (96 games) archived on the dev box
  at tournaments/v020-vs-v010.pgn (box-local; full PGN vendoring into git
  starts at v0.4.0 pool matches).
- Game sample: Ruy Lopez book → 1.e4 e5 2.Nf3 Nc6 3.Bb5 a6 4.Bxc6 bxc6
  5.Nc3 a5?! 6.Nxe5 a4?! 7.d4 a3?! 8.Qh5 axb2?? 9.Qxf7# (v0.1.0 @100ms is
  effectively random among equal scores — expected, it has no ID/TT).
- Conclusion: v0.2.0 search overwhelmingly stronger than the plain-AB
  baseline (lower 95% bound +514 Elo). EXP-0002 strength gate: PASSED.
