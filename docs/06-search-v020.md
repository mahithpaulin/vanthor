# 06 — Search design, v0.2.0 (EXP-0002)

Stack: iterative deepening + aspiration windows (25cp, widen ×6 on fail) →
PVS → TT cutoffs/move ordering → RFP → NMP → LMR → quiescence (captures +
evasions, SEE<0 pruned, TT cutoffs). Check extension +1. Mate-distance
pruning. Killers (2/ply) + butterfly history (depth², halved on overflow).
Lazy SMP (shared lock-free TT, shared best by completed depth).
Reverse-futility margins: 150·depth (d≤3). NMP: depth≥3, eval≥beta,
non-pawn material present, R = 3 + (depth>6). LMR: depth≥3, 4th+ move,
quiet, not in check, killers unreduced; log-table, +1 extra in non-PV.
Single-reply and 50-move draws scored 0; repetitions≥2 scored 0.

Ablation switches (UCI options, all default true): NullMove, LMR, RFP,
CheckExt. Use `go nodes` for deterministic A/B comparisons.

Measured (2026-10-10, 2-core x86 box, single thread):
- bench (d10 startpos + d8 Kiwipete): 624,518 nodes, 0.26s (~2.4 Mnps).
- WAC-new @200k nodes: 239/300 (79.7%).
- v0.1.0 (plain AB) loses on time/depth parity by construction; strength
  delta measured by fixed-match self-play (see tournament log).

Deliberately deferred: forward futility pruning, countermove history,
singular extensions, Syzygy, contempt, ponder, mate-score UCI output.
