#!/usr/bin/env python3
"""Tactical-suite runner: drive any UCI engine over an EPD suite (stdlib only).

Scores a position SOLVED if the engine's bestmove (within the node budget)
matches one of the `bm` answers. Prints solved/total + per-position misses.
Exit 0 always (informational); CI gates via --min-solved.
"""
import argparse
import re
import subprocess
import sys


def parse_epd(path):
    cases = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            m = re.match(r"^(\S+ \S+ \S+ \S+) .*?\bbm\s+([^;]+);", line)
            if not m:
                continue
            fen_core, bm = m.group(1), m.group(2).strip()
            answers = bm.split()
            idm = re.search(r'\bid\s+"?([^";]+)', line)
            cases.append((fen_core + " 0 1", answers, idm.group(1) if idm else "?"))
    return cases


def san_to_uci_move(engine, fen, answer):
    # bm answers are in SAN (e.g. Qg6, Rxb2, O-O). Resolve via a helper:
    # ask the engine to list legal moves? UCI has no such command, so map
    # SAN->UCI with our own minimal parser is error-prone. Instead, accept a
    # match if the engine's bestmove target square + piece match loosely:
    # implemented by comparing against python-chess when available, else
    # require exact UCI answers only.
    return answer


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--engine", required=True)
    ap.add_argument("--epd", required=True)
    ap.add_argument("--nodes", type=int, default=200000)
    ap.add_argument("--threads", type=int, default=1)
    ap.add_argument("--min-solved", type=int, default=0)
    ap.add_argument("--max-positions", type=int, default=0)
    args = ap.parse_args()

    try:
        import chess
        import chess.engine
        have_chess = True
    except ImportError:
        have_chess = False

    cases = parse_epd(args.epd)
    if args.max_positions:
        cases = cases[:args.max_positions]
    print(f"suite: {args.epd} positions={len(cases)} nodes={args.nodes} threads={args.threads}")

    if have_chess:
        eng = chess.engine.SimpleEngine.popen_uci(args.engine)
        eng.configure({"Threads": args.threads, "Hash": 64})
        solved, tested = 0, 0
        misses = []
        for fen, answers, pid in cases:
            board = chess.Board(fen)
            try:
                res = eng.play(board, chess.engine.Limit(nodes=args.nodes))
            except chess.engine.EngineError as e:
                print(f"ENGINE ERROR at {pid}: {e}")
                break
            uci = res.move.uci() if res.move else "0000"
            # resolve SAN answers to UCI via python-chess
            want = set()
            for a in answers:
                try:
                    want.add(board.parse_san(a).uci())
                except ValueError:
                    if re.match(r"^[a-h][1-8][a-h][1-8][qrbn]?$", a):
                        want.add(a)
            ok = uci in want
            tested += 1
            if ok:
                solved += 1
            else:
                misses.append(f"{pid}: got {uci}, want {sorted(want)} [{fen}]")
        try:
            eng.quit()
        except Exception:
            pass
    else:
        # Fallback without python-chess: only exact-UCI bm answers count.
        proc = subprocess.Popen([args.engine], stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, text=True, bufsize=1)
        solved, tested = 0, 0
        misses = []
        proc.stdin.write("uci\n")
        proc.stdin.flush()
        while proc.stdout.readline().strip() != "uciok":
            pass
        proc.stdin.write(f"setoption name Threads value {args.threads}\n")
        proc.stdin.write("isready\n")
        proc.stdin.flush()
        while proc.stdout.readline().strip() != "readyok":
            pass
        for fen, answers, pid in cases:
            uci_answers = [a for a in answers
                           if re.match(r"^[a-h][1-8][a-h][1-8][qrbn]?$", a)]
            proc.stdin.write(f"position fen {fen}\ngo nodes {args.nodes}\n")
            proc.stdin.flush()
            got = None
            while True:
                line = proc.stdout.readline()
                if not line:
                    break
                m = re.match(r"^bestmove (\S+)", line)
                if m:
                    got = m.group(1)
                    break
            tested += 1
            if got and got in uci_answers:
                solved += 1
            else:
                misses.append(f"{pid}: got {got}, want {uci_answers} [{fen}]")
        proc.stdin.write("quit\n")
        proc.stdin.flush()
        proc.wait(timeout=10)
    print(f"solved {solved}/{tested} ({100.0*solved/tested:.1f}%)")
    for m in misses[:30]:
        print("miss " + m)
    if misses and len(misses) > 30:
        print(f"... and {len(misses)-30} more misses")
    if solved < args.min_solved:
        print(f"FAIL: solved {solved} < min {args.min_solved}")
        return 1
    print("RESULT: PASS" if tested else "RESULT: NOPOS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
