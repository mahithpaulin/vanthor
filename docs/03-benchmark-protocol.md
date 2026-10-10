# 03 — Benchmark protocol (preregistered primary + statistics)

## 3.1 Primary benchmark (frozen before the title match)

| Field | Value |
|---|---|
| White/Black engines | VANTHOR candidate (frozen sha) vs Stockfish 19 `sf_19` (`edb0d9d`) |
| Stockfish source | official-stockfish/Stockfish tag `sf_19`; binary built with `make -j ARCH=x86-64-avx2` (record full `compiler --version`, flags, sha256) |
| Hardware | GitHub Actions `ubuntu-latest` runner (record `/proc/cpuinfo` model, 2 vCPU visible); sensitivity run on Codespaces 2-core box if approved |
| Threads / Hash | 2 threads each / 256 MB each (equal; any deviation justified + reported) |
| NN / TB | Default embedded nets both sides; no Syzygy unless both sides get identical paths (report) |
| Time control | **40 moves in 120s + 1s increment** (primary). Secondaries: 10s+0.1s blitz, 300s+3s long |
| Openings | Documented PGN suite (e.g. UHO or TCEC-style), **paired colors**: each opening played twice with sides reversed |
| Adjudication | Draw: 5-fold default off; use cutechess `-draw movenumber=40 movecount=8 score=10`; resign `movecount=4 score=900`; TB/adjudication logged |
| Sample size | Minimum **2000 paired games** (1000 openings × 2) for primary; stop early only on predeclared SPRT bounds |
| Exclusions | Defined before: crashes/time-forfeits/illegal moves count as **losses**, never silently dropped; only duplicate/duplicate-opening infra errors excluded, each listed |
| Logs | All PGNs + engine stderr + cutechess output + cpuinfo + binary sha256 in `tournaments/<id>/` |

## 3.2 Statistics

Score fraction `S = (W + D/2) / N`. Elo difference estimate:

```
ΔE = -400 * log10(1/S - 1)
```

Report: W/D/L, N, S, ΔE, **95% CI on ΔE** (normal approx on S, transformed;
note paired-opening correlation as limitation), method + α, color/opening
balance, TC/hardware, exclusions. Superiority criterion (predeclared):
**lower 95% bound on ΔE > 0** (practical target: > +10). Anything else is
reported as inconclusive — no HARKing, no cherry-picked re-runs.

Sample-size guide (85% draws assumed): ±20 Elo needs ~1000 games; ±10 Elo
needs ~4000. Hence the 2000-game minimum as a compromise; extend to 4000+
if the CI straddles zero.

Development SPRT (H0: 0 Elo, H1: +15 Elo, α=β=0.05) gates every promotion;
the primary match is a fixed-size confirmation, not a repeated-peek loop.

## 3.3 Rating pool (secondary)

Opponents: PlentyChess 7/8, Obsidian 16.x, Alexandria 9, Berserk 14,
Viridithas 20, Stormphrax 8, Lc0 (GPU event only), plus VANTHOR champion.
Rate with Ordo (documented) or BayesElo; never mix lists. Report per-opponent
splits, N, pool date, provisional flags, TC/hardware sensitivity.

## 3.4 Fairness

Equal threads/hash/compute by default. If equal threads misrepresents equal
silicon (e.g. AVX-512 vs NEON), state the allocation policy and its limits
up front. Never asymmetric hash, never different hardware without accounting.
