# 05 — Perft verification (v0.1.0)

Move generation is validated by exact perft node counts on six canonical
positions (mate/stalemate/castling/EP/promotion/pin coverage), each matched
exactly, at two depths:

| # | FEN (short) | d (CI) | nodes | d (deep) | nodes |
|---|---|---|---|---|---|
| 0 | startpos | 4 | 197281 | 5 | 4865609 |
| 1 | Kiwipete | 3 | 97862 | 4 | 4085603 |
| 2 | pos3 (EP/pins) | 4 | 43238 | 5 | 674624 |
| 3 | pos4 (promos) | 3 | 9467 | 4 | 422333 |
| 4 | pos5 | 3 | 62379 | 4 | 2103487 |
| 5 | pos6 | 3 | 89890 | 4 | 3894594 |

Method: `tests/test_perft.py` drives the `perft` command and compares exact
counts (CI job, Release + ASan/UBSan). Full FENs are in the test file.

Cross-checks:
- Every count additionally verified against **python-chess** legal move
  generation on the dev machine; `divide` output diffed per-move when a
  mismatch was found (this caught a runtime magic-search failure for
  rook-E1 — see below).
- `keytest 50`: incremental Zobrist key == recomputed key after every ply
  of deterministic pseudo-random games, plus FEN round-trip stability.

Magic bitboards: numbers are found at startup by a deterministic search
(fixed seeds), each table then **exhaustively verified** against direct ray
computation for every occupancy subset; any square that fails falls back to
direct computation with an stderr notice. No unverified magic is ever used
(this policy was added after the rook-E1 incident: a 1M-try search budget
silently returned magic=0, collapsing that square's rays to adjacent
squares; perft caught it at 89529 vs 89890).

Reproduce: `printf 'position fen <F>\\nperft <d>\\nquit\\n' | ./build/vanthor`
