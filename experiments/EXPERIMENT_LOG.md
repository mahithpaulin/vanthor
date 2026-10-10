# Experiment log — every change, kept or reverted, with evidence.

| ID | Date | Hypothesis | Baseline → candidate | Test | Result | Decision |
|---|---|---|---|---|---|---|
| EXP-0000 | 2026-10-10 | Scaffold CI catches UCI regressions | none → v0.0.1 skeleton | CI: build + test_uci + elo self-check + tournament self-test | pending first CI run | — |
| EXP-0001 | — | Full legal movegen passes perft parity | v0.0.1 → v0.1.0 types/bitboard/position/movegen | perft 6 positions vs python-chess + ASan/UBSan | not started | — |

Rules: one major factor at a time; SPRT for strength claims; negative
results stay in this table.
