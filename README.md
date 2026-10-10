# PROJECT VANTHOR

Autonomous chess-engine research: design, implement, optimize, and
competitively validate a high-performance UCI chess engine capable of
being tested against **Stockfish 19** under preregistered, reproducible
conditions.

**Status (2026-10-10): v0.2.0 — real search (ID + aspiration + PVS + TT +
killers/history + NMP/LMR/RFP + check extensions + Lazy SMP). WAC 239/300
@200k nodes; beats v0.1.0 92-4-0 (+669 Elo, lower 95% bound +514) in
paired-book play. Still HCE-only and far from Stockfish — next is v0.3.0
(NNUE research). No rating claims are made.**

## Quick start

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/vanthor            # UCI loop; try: uci / isready / quit
python3 tests/test_uci.py --engine ./build/vanthor
python3 tools/elo.py --wins 0 --draws 0 --losses 0   # usage / self-check
python3 tools/tournament.py --self-test
```

## Layout

| Path | Contents |
|---|---|
| `src/` | C++17 engine (UCI skeleton; search/eval land in v0.1.0+) |
| `tests/` | UCI smoke test (stdlib only) |
| `tools/` | Tournament runner skeleton + Elo/CI calculator |
| `docs/` | 01 survey, 02 architecture, 03 benchmark protocol, 04 roadmap |
| `experiments/` | Experiment log (every change recorded, kept/reverted with evidence) |
| `.github/workflows/` | CI: build + UCI smoke + tools self-test on every push |

## Reproduce

See `REPRODUCE.md`. Every benchmark records exact versions, binary
hashes, hardware, time control, threads/hash, openings, and exclusions —
per `docs/03-benchmark-protocol.md`.

## Benchmark target

Primary opponent: Stockfish 19, tag `sf_19`, commit
`edb0d9db6731067ec50ce619ff372b463bc4dd5d` (2026-09-05, SFNNv16).
Do not substitute another version and label it SF19.

## License

GPL-3.0-only (see `LICENSE`). All surveyed reference engines are GPL-3.0
(or GPL-3.0 with commercial NNUE exceptions — see survey). VANTHOR is an
independent implementation; provenance of every imported idea is logged.
