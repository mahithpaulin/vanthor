# 04 — Roadmap (phases, milestones, compute, uncertainties)

## Milestones (evidence-gated)

- [x] **A — Audit + survey** (this commit): env limits, SF19 pinned, pool chosen.
- [ ] **B — Test infra** (v0.0.1, this commit → CI green): UCI skeleton, elo/tournament tools self-tests, perft harness stub.
- [ ] **v0.1.0 Correctness**: bitboards, full legal movegen, perft parity (6 positions vs python-chess), HCE v1, fixed-depth determinism. Gate: perft + ASan/UBSan + STS smoke.
- [ ] **v0.2.0 Search**: ID + PVS + QSearch + TT + ordering + NMP/LMR/LMP + time mgmt + Lazy SMP(2t). Gate: SPRT +15 vs v0.1.0, WAC > 80%.
- [ ] **v0.3.0 NNUE**: compact HalfKP trainer on Kaggle T4 (teacher: SF19 evals on self-play positions, deduped, train/val/test split); CPU AVX2 inference; HCE vs NNUE match. Gate: NNUE beats HCE by SPRT.
- [ ] **v0.4.0 Competitive**: 2000-game primaries vs SF19 + pool rounds; analysis loop (loss taxonomy → targeted fixes).
- [ ] **v1.0 Title match**: frozen candidate, preregistered 2000–4000 games, full dossier (PGNs, ratings, repro).

## Compute budget (free-tier honest)

- GitHub Actions (public repo = unlimited minutes): all builds, perft, UCI,
  short matches, tool tests. Long matches sharded across jobs (each < 30 min).
- Kaggle T4 (30 GPU-h/week, 12h/session, checkpoint always): NNUE training
  only; data prep on CPU. Never train on the phone or the 2-core box.
- Codespaces box (45 h left, floor 5 h, needs explicit start approval):
  debugging/interactive only, last resort. **Not used unless approved.**
- Phone: orchestration + docs only (1 GiB RAM; no builds/training locally).
- Artifacts: GitHub Releases (binaries+manifests), Kaggle datasets (training
  data/checkpoints), Drive `Chess/` (PGN mirrors). No secrets anywhere.

## Top uncertainties → experiments

1. **Data scale gap**: SF trains on >100B positions; we get ~30 T4-h/week.
   EXP: distill SF19-eval labels onto 10–50M self-play positions; test whether
   small-net prediction gain converts to playing Elo (often it doesn't).
2. **SMP on 2 threads**: shared-history benefit unproven at this scale.
   EXP: Lazy SMP on/off at 2t, measure NPS scaling + SPRT.
3. **Threat/pawn-pair features**: proven for SF/PlentyChess, cost = speed.
   EXP: ablate features vs NPS vs Elo on fixed TC.
4. **GPU relevance**: Lc0-class strength needs GPU; our title match is CPU.
   EXP: separate GPU exhibition event; don't conflate lists.
5. **Ethereal licensing**: commercial nets excluded — verify Standard-only or
   drop Ethereal from pool (decision logged before use).
