#!/bin/bash
# VANTHOR continuous-learning daemon (runs ON THE BOX under nohup).
# One round = fresh data -> append corpus -> Kaggle T4 train -> verify ->
# candidate-vs-champion match -> promote-or-log. Idempotent via STATE file;
# safe to kill and restart. Stops on learn/STOP, round cap, or GPU floor.
#
# Usage:  bash tools/autolearn.sh  (from repo root; logs to learn/daemon.log)
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LEARN="$ROOT/learn"
KAGGLE=/home/codespace/venv/bin/kaggle
ENGINE="$ROOT/build/vanthor"
SPAR="$ROOT/learn/v101bin"   # v0.1.0 sparring binary (mixed-strength data)
TEACHER=/workspaces/sf19-src/src/stockfish
MAX_ROUNDS=10
GPU_FLOOR_H=4
MIN_CORPUS=20000   # positions needed before first training attempt

mkdir -p "$LEARN/nets" "$LEARN/kdata" "$LEARN/kernel" "$LEARN/reports"
LOG="$LEARN/daemon.log"
STATE="$LEARN/STATE"

log() { echo "$(date -u +%FT%TZ) $*" | tee -a "$LOG"; }
get_state() { [ -f "$STATE" ] && cat "$STATE" || echo "ROUND=1 PHASE=INGEST"; }
set_state() { echo "ROUND=$1 PHASE=$2" > "$STATE"; }

# ---- one-time setup -------------------------------------------------------
if [ ! -f "$LEARN/CHAMPION.json" ]; then
    echo '{"round":0,"kind":"HCE","net":null,"note":"v0.2.0 HCE baseline"}' > "$LEARN/CHAMPION.json"
fi
if [ ! -x "$SPAR" ] && [ -d /workspaces/v101 ]; then
    cp /workspaces/v101/build/vanthor "$SPAR" 2>/dev/null || true
fi
touch "$LEARN/corpus.txt"

# adopt + retire any pre-existing manual data-gen streams (keep their data)
for f in /tmp/dataA.txt /tmp/dataB.txt; do
    if [ -f "$f" ]; then
        cat "$f" >> "$LEARN/corpus.txt"
        log "ingested pre-existing $f"
    fi
done
pkill -f "gen_data.py --engine ./build/vanthor" 2>/dev/null || true
log "retired manual streams; corpus=$(wc -l < "$LEARN/corpus.txt") lines"

ROUND=1; PHASE="INGEST"
if [ -f "$STATE" ]; then eval "$(cat "$STATE")"; fi

while [ "$ROUND" -le "$MAX_ROUNDS" ]; do
    [ -f "$LEARN/STOP" ] && { log "STOP file present: exiting"; exit 0; }

    # ---- INGEST: two fresh 400-game streams (mixed + self), then append ----
    if [ "$PHASE" = "INGEST" ]; then
        log "=== round $ROUND INGEST ==="
        cd "$ROOT"
        HEAD_BEFORE=$(git rev-parse HEAD)
        for i in 1 2 3; do
            if git pull 2>&1 | tail -n2 >> "$LOG"; then break; fi
            log "git pull attempt $i failed; retrying"
            sleep 20
        done
        log "HEAD=$(git rev-parse --short HEAD)"
        if [ "$(git rev-parse HEAD)" != "$HEAD_BEFORE" ]; then
            log "updated to new HEAD: re-exec to run the new code cleanly"
            exec bash tools/autolearn.sh
        fi
        cmake --build build -j2 2>&1 | tail -n1 >> "$LOG"
        python3 tools/gen_data.py --engine "$ENGINE" --engine2 "$SPAR" \
            --teacher "$TEACHER" --games 400 --movetime 30 --depth 8 \
            --out "$LEARN/round_${ROUND}_mix.txt" >> "$LOG" 2>&1 &
        P1=$!
        python3 tools/gen_data.py --engine "$ENGINE" \
            --teacher "$TEACHER" --games 400 --movetime 30 --depth 8 \
            --out "$LEARN/round_${ROUND}_self.txt" >> "$LOG" 2>&1 &
        P2=$!
        wait $P1; S1=$?; wait $P2; S2=$?
        log "gen streams done rc=$S1/$S2"
        cat "$LEARN/round_${ROUND}_mix.txt" "$LEARN/round_${ROUND}_self.txt" >> "$LEARN/corpus.txt"
        tail -n 400000 "$LEARN/corpus.txt" > "$LEARN/corpus.tmp" && mv "$LEARN/corpus.tmp" "$LEARN/corpus.txt"
        N=$(wc -l < "$LEARN/corpus.txt")
        log "corpus=$N lines"
        set_state "$ROUND" TRAIN; PHASE=TRAIN
    fi

    # ---- TRAIN on Kaggle T4 ------------------------------------------------
    if [ "$PHASE" = "TRAIN" ]; then
        log "=== round $ROUND TRAIN ==="
        N=$(wc -l < "$LEARN/corpus.txt")
        GPU_LEFT=$($KAGGLE quota 2>/dev/null | awk '$1=="GPU"{print $3}' | tr -d 'h')
        log "corpus=$N gpu_left=${GPU_LEFT:-?}h"
        if [ "$N" -lt "$MIN_CORPUS" ]; then
            log "corpus below minimum: skipping to next ingest"
            ROUND=$((ROUND+1)); set_state "$ROUND" INGEST; PHASE=INGEST; continue
        fi
        if [ -n "${GPU_LEFT:-}" ] && awk "BEGIN{exit !($GPU_LEFT < $GPU_FLOOR_H)}"; then
            log "GPU below floor: matches/analysis only this round"
            set_state "$ROUND" VERIFY_MATCH; PHASE=VERIFY_MATCH
        else
            cp "$LEARN/corpus.txt" "$LEARN/kdata/train.txt"
            if [ ! -f "$LEARN/kdata/dataset-metadata.json" ]; then
                "$KAGGLE" datasets init -p "$LEARN/kdata" >> "$LOG" 2>&1
                python3 - "$LEARN/kdata/dataset-metadata.json" >> "$LOG" 2>&1 <<'EOF'
import json, sys
p = sys.argv[1]
m = json.load(open(p))
m["id"] = "mahithpaulin/vanthor-train"
m["title"] = "vanthor-train"
json.dump(m, open(p, "w"), indent=2)
print("dataset id set")
EOF
                "$KAGGLE" datasets create -p "$LEARN/kdata" >> "$LOG" 2>&1 || true
            else
                "$KAGGLE" datasets version -p "$LEARN/kdata" -m "round $ROUND" >> "$LOG" 2>&1 || true
            fi
            SLUG="vanthor-net-r$ROUND"  # NOTE: kernel slug must differ from
            # the dataset slug (Kaggle 409s otherwise): dataset=vanthor-train
            cp "$ROOT/tools/train_nnue.py" "$LEARN/kernel/kernel.py"
            cat > "$LEARN/kernel/kernel-metadata.json" <<EOF
{"id": "mahithpaulin/$SLUG", "title": "$SLUG", "code_file": "kernel.py",
 "language": "python", "kernel_type": "script", "is_private": true,
 "enable_gpu": true, "enable_internet": true, "session_timeout_seconds": 10800,
 "dataset_sources": ["mahithpaulin/vanthor-train"]}
EOF
            # Kernel runs bare: train_nnue.py autodetects /kaggle/input paths
            # and its defaults (H=128, 60 epochs, batch 16384) are the recipe.
            cd "$LEARN/kernel"
            "$KAGGLE" kernels push -p . --accelerator NvidiaTeslaT4 >> "$LOG" 2>&1 || {
                log "kernel push failed; will retry next round"; cd "$ROOT"
                ROUND=$((ROUND+1)); set_state "$ROUND" INGEST; PHASE=INGEST; continue; }
            cd "$ROOT"
            log "kernel mahithpaulin/$SLUG pushed; polling"
            for _ in $(seq 1 240); do
                sleep 120
                [ -f "$LEARN/STOP" ] && { log "STOP during train poll"; exit 0; }
                ST=$("$KAGGLE" kernels status "mahithpaulin/$SLUG" 2>/dev/null | tail -n1)
                log "kernel status: $ST"
                case "$ST" in
                    *COMPLETE*) break ;;
                    *ERROR*|*FAILED*|*CANCELLED*) ST="FAILED"; break ;;
                esac
            done
            if [ "$ST" != "FAILED" ]; then
                rm -rf "$LEARN/kernel_out" && mkdir -p "$LEARN/kernel_out"
                "$KAGGLE" kernels output "mahithpaulin/$SLUG" -p "$LEARN/kernel_out" --force >> "$LOG" 2>&1 || true
                if [ -f "$LEARN/kernel_out/net.bin" ]; then
                    head -c 5 "$LEARN/kernel_out/net.bin" | grep -q VNTH1 \
                        && cp "$LEARN/kernel_out/net.bin" "$LEARN/nets/candidate_r${ROUND}.bin" \
                        && log "candidate net saved" \
                        || log "candidate net FAILED magic check"
                else
                    log "no net.bin in kernel output"
                fi
            else
                log "kernel run failed"
            fi
            unset VNTH_DATA VNTH_OUT VNTH_HIDDEN VNTH_EPOCHS VNTH_BATCH
            set_state "$ROUND" VERIFY_MATCH; PHASE=VERIFY_MATCH
        fi
    fi

    # ---- VERIFY + MATCH ----------------------------------------------------
    if [ "$PHASE" = "VERIFY_MATCH" ]; then
        log "=== round $ROUND VERIFY+MATCH ==="
        CAND="$LEARN/nets/candidate_r${ROUND}.bin"
        if [ ! -f "$CAND" ]; then
            log "no candidate net: analysis-only round (WAC vs HCE baseline)"
            python3 tools/run_suite.py --engine "$ENGINE" --epd tests/wacnew.epd \
                --nodes 200000 --max-positions 60 >> "$LOG" 2>&1 || true
            ROUND=$((ROUND+1)); set_state "$ROUND" INGEST; PHASE=INGEST; continue
        fi
        printf 'setoption name EvalFile value %s\nnnue_verify 10\nquit\n' "$CAND" \
            | timeout 300 ./build/vanthor > "$LEARN/reports/verify_r${ROUND}.txt" 2>&1
        grep -q "nnue_verify ok" "$LEARN/reports/verify_r${ROUND}.txt" \
            && log "nnue_verify ok" \
            || { log "nnue_verify FAILED: candidate rejected"; ROUND=$((ROUND+1)); set_state "$ROUND" INGEST; PHASE=INGEST; continue; }
        CHAMP_KIND=$(python3 -c "import json;print(json.load(open('$LEARN/CHAMPION.json'))['kind'])")
        if [ "$CHAMP_KIND" = "HCE" ]; then
            CHAMP_OPTS=(--opt-b UseNNUE=false)
        else
            CHAMP_OPTS=(--opt-b UseNNUE=true "--opt-b EvalFile=$LEARN/nets/champion.bin")
        fi
        # Engine A (--white binary) = candidate in every game; colors alternate
        # via the paired-book schedule, so FINAL W/D/L mixes both colors.
        python3 tools/play_match.py --white "$ENGINE" --black "$ENGINE" \
            --games 96 --movetime 100 --threads 1 --hash 32 \
            --opt-a UseNNUE=true "--opt-a EvalFile=$CAND" \
            "${CHAMP_OPTS[@]}" \
            --pgn "$LEARN/reports/match_r${ROUND}.pgn" \
            --name-white "candidate-r$ROUND" --name-black "champion" \
            > "$LEARN/reports/match_r${ROUND}.log" 2>&1 || true
        tail -n 3 "$LEARN/reports/match_r${ROUND}.log" >> "$LOG"
        set_state "$ROUND" PROMOTE; PHASE=PROMOTE
    fi

    # ---- PROMOTE decision --------------------------------------------------
    if [ "$PHASE" = "PROMOTE" ]; then
        log "=== round $ROUND PROMOTE ==="
        if [ ! -f "$LEARN/reports/match_r${ROUND}.pgn" ]; then
            log "PROMOTE skipped: no match PGN (match step failed, see match log)"
        else
        python3 - "$LEARN" "$ROUND" >> "$LOG" 2>&1 <<'EOF'
import json, re, sys, subprocess
learn, rnd = sys.argv[1], sys.argv[2]
pgn = f"{learn}/reports/match_r{rnd}.pgn"
cw = cd_ = cl = 0  # candidate as White
cb_w = cb_d = cb_l = 0  # candidate as Black (from Black's perspective invert)
cur_white = None
for line in open(pgn):
    m = re.match(r'\[White "(.*)"\]', line)
    if m: cur_white = m.group(1); continue
    m = re.match(r'\[Result "(.*)"\]', line)
    if m and cur_white:
        r = m.group(1)
        cand_white = cur_white.startswith("candidate")
        if r == "1-0":
            if cand_white: cw += 1
            else: cb_l += 1
        elif r == "0-1":
            if cand_white: cl += 1
            else: cb_w += 1
        else:
            if cand_white: cd_ += 1
            else: cb_d += 1
        cur_white = None
# candidate totals regardless of color
W = cw + cb_w; D = cd_ + cb_d; L = cl + cb_l
out = subprocess.run([sys.executable, "tools/elo.py", "--wins", str(W),
                      "--draws", str(D), "--losses", str(L)],
                     capture_output=True, text=True, cwd=".")
print(f"candidate (color-balanced): W={W} D={D} L={L}")
print(out.stdout.strip())
elo_line = [l for l in out.stdout.splitlines() if l.startswith("dE=")][0]
m = re.search(r"dE=(-?[\d.]+) 95%CI=\[(-?[\d.]+),(-?[\d.]+)\]", elo_line)
pt, lo, hi = map(float, m.groups())
rule = (pt > 0) and (lo > -25)
print(f"PROMOTE={rule} (rule: point>0 and lower>-25)")
ch = json.load(open(f"{learn}/CHAMPION.json"))
if rule:
    import shutil
    shutil.copy(f"{learn}/nets/candidate_r{rnd}.bin", f"{learn}/nets/champion.bin")
    json.dump({"round": int(rnd), "kind": "NNUE",
               "net": f"learn/nets/champion.bin",
               "match": f"W={W} D={D} L={L} dE={pt} lo={lo}",
               "val_net": f"candidate_r{rnd}.bin"}, open(f"{learn}/CHAMPION.json", "w"), indent=2)
    print("promoted")
else:
    ch["last_rejected"] = {"round": int(rnd), "match": f"W={W} D={D} L={L} dE={pt} lo={lo}"}
    json.dump(ch, open(f"{learn}/CHAMPION.json", "w"), indent=2)
    print("kept champion")
EOF
        fi
        ROUND=$((ROUND+1)); set_state "$ROUND" INGEST; PHASE=INGEST
    fi
done
log "round cap reached ($MAX_ROUNDS): exiting for human review"
exit 0
