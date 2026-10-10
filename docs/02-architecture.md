# 02 — VANTHOR architecture (v0.0.1 scaffold → v0.1.0 baseline)

## Language decision

- Engine: **C++17** (later C++20 if CI proves it). Reasons: reference
  ecosystem (SF/PlentyChess/Obsidian/Alexandria all C++), SIMD intrinsics,
  UCI/syzygy examples, fastest CPU NNUE path. Rust was considered
  (Reckless/Cinder prove viability) but C++ keeps the NNUE trainer +
  inference literature directly applicable.
- Training/orchestration/analysis: **Python 3** (stdlib + torch on Kaggle
  only; phone/CI use stdlib + python-chess where pure-python).
- tournament runner: Python wrapping cutechess-cli.

## Module map

```
src/
  version.h     compile-time identity (name, version, git sha)
  types.h       (v0.1.0) color/piece/square/score/move, static asserts
  bitboard.h    (v0.1.0) attack tables, magics/sliders, popcount
  position.h    (v0.1.0) board state, make/unmake, zobrist, repetition, 50-move
  movegen.h     (v0.1.0) legal generation, SEE
  search.h      (v0.2.0) ID + PVS + QSearch + TT + ordering + pruning/LMR
  eval.h        (v0.1.0 HCE → v0.3.0 NNUE) tapered, PST, mobility, pawns, king safety
  nnue/         (v0.3.0) feature transformer + affine stack, incremental update, SIMD
  threads.h     (v0.2.0) Lazy SMP, shared histories (PlentyChess-style)
  time.h        (v0.2.0) time management, abort plumbing
  uci.h/.cpp    (v0.0.1 DONE) protocol loop; search callback injected later
  bench.h       (v0.0.1 stub → v0.2.0 real node/speed benchmark)
```

v0.0.1 (this commit): `uci.cpp` + `main.cpp` only. `go` returns a
deterministic stub move and says so on stderr. This is scaffolding, not
an engine. v0.1.0 adds types/bitboard/position/movegen + perft + HCE.

## Key design constraints

1. Correctness first: perft parity with python-chess on 6+ positions,
   make/unmake differential tests, sanitizers (ASan/UBSan) in CI.
2. One-factor-at-a-time: every search/eval change behind a UCI option or
   branch + SPRT vs champion (see 03-benchmark-protocol).
3. Determinism hooks: fixed-hash, single-thread, fixed-depth modes for
   regression; seeds recorded where randomness exists.
4. NNUE from day one in the design: accumulator layout + refresh/update
   API frozen at v0.1.0 so HCE→NNUE swap doesn't destabilize search.
5. Hash/threads fairness: identical allocation vs SF19 in primary tests;
   document any justified deviation.

## Build matrix (CI)

- ubuntu-latest × {g++, clang++} × Release; `-O3 -march=x86-64 -msse4.1`
  baseline, arch-specific (`avx2`, `avx512vnni`, `neon`) release artifacts
  with sha256 manifests. Native `-march=native` never in releases.
