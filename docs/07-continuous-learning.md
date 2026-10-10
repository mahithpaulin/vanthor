# 07 — Continuous learning (autolearn daemon + engine persistence)

## Two learning levels

1. **Loop level (big gains):** `tools/autolearn.sh` runs on the dev box,
   one round = fresh self-play data → append corpus (cap 400k) → Kaggle T4
   train (HalfKP-128, 60 epochs) → `nnue_verify` + WAC → candidate-vs-champion
   match (96 games, paired book) → promote-or-log. Champion tracked in
   `learn/CHAMPION.json` (round 0 = HCE v0.2.0).
2. **Engine level (small, always-on):** shared butterfly history persists
   across searches and, with `PersistHistory` + `HistoryFile`, across
   sessions (32 KB `VNTHHIST1` file, auto-saved on quit, `savehistory`
   command). Default OFF — benchmarks run with a clean slate for fairness.

## Promotion rule (dev loop)

Promote iff candidate point-estimate dE > 0 AND lower 95% bound > −25
(lenient: keeps weak-signal progress; the STRICT lower>0 gate is reserved
for the v1.0 title match). Every decision + match log is kept win or lose.

## Guardrails (unsupervised operation)

- `learn/STOP` file halts the daemon after the current step.
- Max 10 rounds, then exit for human review.
- New GPU training only if Kaggle GPU remaining ≥ 4 h (30 h/week budget).
- Phone watchdog (`~/bin/vanthor-watch`, 15-min polls) keeps the box out
  of idle-stop during work and writes STOP when Codespaces quota < 10 h.
- `learn/` (nets, corpus, reports) is box-local and git-ignored; promotion
  manifests go in the daemon log (ported to the repo log on review).

## Stop everything

- Box: `touch /workspaces/vanthor/learn/STOP` (daemon exits gracefully).
- Phone: `kill $(cat ~/tmp/vanthor-watch.pid)` + `termux-wake-unlock`.
