# Experiment log — every change, kept or reverted, with evidence.

| ID | Date | Hypothesis | Baseline → candidate | Test | Result | Decision |
|---|---|---|---|---|---|---|
| EXP-0000 | 2026-10-10 | Scaffold CI catches UCI regressions | none → v0.0.1 skeleton | CI: build + test_uci + elo self-check + tournament self-test | **PASS** — run 38033707022, 14s, g++ + clang++ builds, UCI smoke PASS, elo self-check PASS, tournament self-test PASS | **KEPT** (scaffold accepted as Phase B base) |
| EXP-0001 | 2026-10-10 | Original bitboard movegen is fully legal | v0.0.1 stub → v0.1.0 types/bitboard/position/movegen | 6-position perft exact ×2 depths + python-chess divide diff + keytest + ASan/UBSan | **PASS** — CI run 38035390840 green (25s); deep perfts to d4/d5 all exact; found+fixed 3 real bugs (missing include, fen+moves parse, magic-search budget → verified tables) | **KEPT**, tagged v0.1.0 |
| EXP-0002 | 2026-10-10 | ID+PVS+TT+ordering+LMR adds strength over plain AB | v0.1.0 → v0.2.0 search | 96-game paired-book match @100ms/move + WAC @200k nodes + SMP smoke | **PASS** — W92 D4 L0, dE +669 (95% lower bound +514); WAC 239/300 (79.7%); 2-thread d10 in 70ms; found+fixed 3 real bugs (TTSlot non-movable, unitbuf pipe deadlock, worker-thread reap SIGABRT) | **KEPT**, closing as v0.2.0 |

Rules: one major factor at a time; SPRT for strength claims; negative
results stay in this table.
