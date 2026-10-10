# 01 — Engine landscape survey (verified 2026-10-10)

Sources: project websites, GitHub metadata/APIs, release notes, CCRL/CEDR
lists. Observations are from public docs; hypotheses are labeled as such.

## Primary benchmark

| Item | Verified fact |
|---|---|
| Stockfish 19 | Released 2026-09-05. Tag `sf_19`, commit `edb0d9d` (`edb0d9db6731067ec50ce619ff372b463bc4dd5d`). github.com/official-stockfish/Stockfish, C++, **GPL-3.0**, ~16.9k stars |
| SF19 chess content | +44 Elo (up to) vs SF18; wins >3x as many game pairs as lost. **SFNNv16**: threat features trimmed, pawn-pair features added, binary smaller. Secondary small net (since 16.1) **retired** |
| SF18 (context) | 2026-01-31. SFNNv10 + threat inputs, correction history in search, shared-memory weights, ~+46 Elo vs SF17 |
| License obligation | GPL-3.0: any distributed derivative must stay GPL-3.0 with source + attribution. Studying ideas is fine; copying files carries the license with them |

## Peer engines (rating order approx, CCRL 40/15 4CPU, Mar–May 2026)

| Engine | Repo / lang / license | Eval / search notes | Relevance to VANTHOR |
|---|---|---|---|
| Reckless 0.9/0.10-dev | Rust, open source | Fast modern AB + NNUE; ~3648, 2nd behind SF18 | Rust viability proof; SMP/model to watch |
| PlentyChess 7/8 | Yoshie2000/PlentyChess, C++, NNUE | **Threat-input NNUE pioneer** (v7 TI), +pawn-pair inputs in v8; fractional depth, shared correction history, NUMA handling | Closest architectural reference for NNUE feature design |
| Torch v4d | Chess.com project | Elite-event regular | Diversity opponent only (availability TBD) |
| Obsidian 16.x | gab8192/Obsidian, C++, **GPL-3.0** | Compact UCI engine, rapid climb to ~3638 | Clean-code reference; opponent |
| Alexandria 9.0 | PGG106/Alexandria, C++, **GPL-3.0** | Bitboard + NNUE-era search/eval | Opponent + readable reference |
| Viridithas 19/20 | open source | Active dev, clear docs, ~3633 | Opponent |
| Berserk 14 | jhonnold/berserk, C, **GPL-3.0** | Null-move/LMR/LMP heritage, ~3628–3784 across lists | C implementation reference; opponent |
| Dragon by Komodo 3.3 | commercial-ish lineage | ~3627 | Probably excluded (licensing/availability) |
| Stormphrax 8, Caissa 1.2x, Cinder 0.6 (Rust), Hobbes 3, pawnocchio 2.0.1 | various open source | NNUE-era, diverse authors | Rating-pool candidates |
| Ethereal 14.x | AndyGrant/Ethereal, C, **GPL-3.0 for code** | AB + NN, clean/digestible reference code. **TRAP: NNUE nets v13+ are commercial ($40); binaries on GitHub are Standard (non-NNUE) and far weaker** | Use Standard only or exclude; never redistribute commercial nets |
| Leela Chess Zero 0.32/0.33-rc0 | LeelaChessZero/lc0, C++, **GPL-3.0** | MCTS/PUCT (+dag-preview), ResNet-SE body, BT3/BT4 transformer nets, CUDA/ONNX/XLA/SYCL backends, T4-relevant | GPU-path reference + training-pipeline ideas; needs GPU to be competitive |
| Integral, Halogen | open source | Mid-3600s | Pool fillers |

Hypothesis (not yet tested): on CPU-only runners the AB+NNUE family
(PlentyChess/Obsidian/Alexandria/Berserk) is the right comparison pool;
Lc0 needs a GPU allocation (Kaggle T4) to show its strength.

## Technique ledger (purpose / cost / evidence)

- Alpha-beta + PVS + iterative deepening: baseline; cost = complexity of
  correct time/abort handling. Evidence: universal.
- Move ordering (hash move, captures/MVV-LVA/SEE, killers, history,
  counter-moves): cheap, large Elo/$; failure mode = pathologies when
  ordering is inverted. Evidence: every surveyed engine.
- Transposition table: +depth/avoid re-search; cost = hashing bugs,
  SMP races. Evidence: universal.
- Quiescence + SEE pruning: tactical stability; cost = explosion control.
- Pruning/reductions (NMP, LMR, LMP, futility, razoring, probcut):
  each needs independent SPRT; stacking unmeasured techniques loses Elo.
- Correction history (SF18-era): adjusts eval from search patterns;
  helps fortresses/stalemates; needs tuning data.
- NNUE eval (HalfKP → threat/pawn-pair inputs, SFNNv10→v16): biggest
  single eval gain; cost = trainer + data scale (SF uses >100B Lc0-data
  positions — far beyond our 30 GPU-h/week T4 budget; plan: distill from
  teacher evals + targeted self-play, small nets first).
- SMP (Lazy SMP / shared histories, NUMA): needed for 2-thread fairness
  tests; measure scaling, don't assume linearity.
- SIMD (SSSE3/AVX2/AVN512-VNNI): inference throughput; build per-arch
  binaries, verify with checksums.

## Tournament / rating tooling (verified choices)

- Match runner: **cutechess-cli** (GPL; UCI, openings, color reversal,
  adjudication, PGN). Fallback: python-chess orchestration.
- Ratings: **BayesElo** or **Ordo** (document choice per event); SPRT for
  development (e.g. `sprt` in cutechess/OpenBench methodology).
- Suites: perft (movegen), WAC/BT/STS (tactics/strategy), UCI compliance.

## Provenance rules

- No copied files without license compatibility + attribution + log entry.
- Ethereal commercial nets: never fetch/redistribute.
- Datasets: record source, license, filtering, label procedure, version.
