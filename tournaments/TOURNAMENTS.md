# Tournament log

## T-0002 — strength anchor vs Stockfish 19 (2026-10-10, box, fixed nodes)

- Vanthor: v0.2.0 HCE (`30cdbbb`-era binary, Threads 1). SF19: `edb0d9d`
  source build (`154ad263…`), Threads 1. Runner: play_match.py fixed-nodes.
- 1:1 (10k:10k), 24 games: **0-0-24**, all checkmates. Unresolvable directly.
- 10:1 (V10k : SF1k), 24 games: **0-3-21**, dE=−470, 95% CI [−3600, −289].
- 50:1 (V10k : SF200), 24 games: **2-10-12**, dE=−154, 95% CI [−364, −18].
- Synthesis: SF self-scaling (~90 Elo/doubling in this range) projects the
  equal-node gap to **≈ −650 ± 150 Elo vs SF19**. With SF19 ≈ 3700 on this
  class of hardware, Vanthor-HCE ≈ **≈3050 ± 150** (engine scale, provisional,
  24-game samples, fixed-nodes ≠ tournament TC — do not cite as a rating).
- Context: zero search tuning done so far; NNUE not yet integrated. The gap
  is the honest baseline the learning loop must close.

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
