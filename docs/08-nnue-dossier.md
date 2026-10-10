# 08 — NNUE training dossier (v0.3.0, living document)

## Recipe v1 (frozen for rounds 1–10)

- Features: HalfKP, 41024 inputs. White list = white-king perspective;
  black list = black-king perspective, king index AND squares flipped, slots
  relative to own color (exact spec in `src/nnue.h`; must match
  `tools/train_nnue.py::halfkp_indices`).
- Model: EmbeddingBag(41024 → H) summed per side; halves ordered by side to
  move; clip [0,1]; Linear(2H → 1). H=128 production (H=16 smoke).
- Targets: teacher cp clamped ±1500 (in pawns) + 0.1 × MSE(sigmoid(pred/0.6),
  game result). Optimizer Adam lr=0.001, cosine schedule, 60 epochs,
  batch 16384, seed fixed, 5% validation split (first rows — corpus is
  SHUFFLED before upload so the split is representative).
- Export: `VNTH1` float32 (ft [41024,H], ft-bias zeros, out_w/out_b, bias).

## Data provenance

- Positions: Vanthor self-play (mixed v0.2.0-dev vs v0.1.0 + self), 24-line
  book, movetime 30ms, sampled ply>16 non-check, ≤40/game, corpus capped
  400k lines (tail). Round 10 corpus ≈ 330k.
- Labels: Stockfish 19 (`edb0d9d`, sha `154ad263…`) fixed depth 8, White
  perspective; mates skipped. Meta (teacher id/sha, depth) in
  `<out>.meta.json` at generation time.
- Limitations (honest): weak-engine positions; result column mostly draws in
  self-play legs; no dedup yet; depth-8 labels are noisy vs deeper ones.
  These bound v1 expectations: prediction gains may not fully convert.

## Infrastructure (all preflight-verified 2026-10-10)

- Dataset `mahithpaulin/vanthor-train` (versioned per round), kernels
  `vanthor-net-rN` (slug MUST differ from dataset slug or push 409s).
- Metadata uses JSON booleans; dataset mounts at
  `/kaggle/input/datasets/<owner>/<slug>/` (probe first, layouts vary).
- Script kernels run bare: trainer autodetects input, defaults are the
  recipe. T4 reproduces box-CPU losses exactly (deterministic stack).
- GPU budget: 30 h/week; training gated on ≥4 h remaining; ~0.5 h/round.

## Results log

- r4–r9: trained + `nnue_verify` ok; matches never ran (runner flag bug).
- r10: in training at time of writing; first candidate-vs-champion match
  pending. Outcomes recorded in `learn/` reports + EXPERIMENT_LOG.
