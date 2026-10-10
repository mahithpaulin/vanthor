#!/usr/bin/env python3
"""Dataset generator for NNUE training (v0.3.0).

Pipeline:
  1. Self-play: ENGINE vs ENGINE fast games from the paired book
     (tools/play_match.py engine, reuse via import or subprocess).
  2. Sample positions: skip first 8 plies (book), skip positions where
     either side is in check evasions-only? (keep all, note stats), skip
     mate scores.
  3. Label: TEACHER engine fixed-depth search score (centipawns, White
     perspective) + game result (1/0/0.5).
  4. Write text format: FEN | score_cp | result   (one per line).

Usage:
  gen_data.py --engine ./build/vanthor --teacher /path/to/sf19
              --games 50 --movetime 50 --depth 10 --out data/raw.txt
Provenance: teacher name/version/sha recorded in OUT.meta.json.
"""
import argparse
import hashlib
import json
import sys
import time

sys.path.insert(0, __import__("os").path.dirname(__import__("os").path.abspath(__file__)))
from play_match import OPENINGS  # noqa: E402


def engine_uci(path, threads=1, hashmb=32):
    import chess.engine
    e = chess.engine.SimpleEngine.popen_uci(path)
    e.configure({"Threads": threads, "Hash": hashmb})
    return e


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--engine", required=True)
    ap.add_argument("--engine2", default="",
                    help="second engine (default: same as --engine); colors alternate each game")
    ap.add_argument("--teacher", required=True)
    ap.add_argument("--teacher-name", default="stockfish19-sf_19")
    ap.add_argument("--games", type=int, default=50)
    ap.add_argument("--movetime", type=int, default=50)
    ap.add_argument("--depth", type=int, default=10)
    ap.add_argument("--out", required=True)
    ap.add_argument("--threads", type=int, default=1)
    args = ap.parse_args()

    import chess
    import chess.engine

    teach = engine_uci(args.teacher, args.threads, 128)
    try:
        tid = teach.id
    except Exception:
        tid = {}
    players = [engine_uci(args.engine, 1, 32)]
    players.append(engine_uci(args.engine2 or args.engine, 1, 32))

    n_pos = n_games = 0
    t0 = time.time()
    out = open(args.out, "w")
    try:
        for g in range(args.games):
            book = OPENINGS[(g // 2) % len(OPENINGS)]
            board = chess.Board()
            for u in book.split():
                board.push_uci(u)
            # alternate colors each game (balances book sides; when
            # --engine2 is given this also mixes opponent strength)
            order = players if g % 2 == 0 else players[::-1]
            result = None
            game_pos = []
            while not board.is_game_over(claim_draw=True) and board.ply() < 400:
                eng = order[0] if board.turn == chess.WHITE else order[1]
                try:
                    res = eng.play(board, chess.engine.Limit(
                        time=args.movetime / 1000.0))
                except (chess.engine.EngineError,
                        chess.engine.EngineTerminatedError):
                    break
                if res.move is None or res.move not in board.legal_moves:
                    break
                board.push(res.move)
                if board.ply() > 16 and not board.is_check():
                    game_pos.append(board.fen())
            if board.is_checkmate():
                result = 0.0 if board.turn == chess.WHITE else 1.0
            elif board.is_game_over(claim_draw=True):
                result = 0.5
            else:
                continue  # crashed/unfinished: drop the game, count nothing
            # label sampled positions (cap per game to bound teacher cost)
            for fen in game_pos[:40]:
                b = chess.Board(fen)
                try:
                    info = teach.analyse(b, chess.engine.Limit(depth=args.depth))
                    sc = info["score"].white()
                    if sc.is_mate():
                        continue
                    cp = sc.score()
                except (chess.engine.EngineError,
                        chess.engine.EngineTerminatedError):
                    continue
                out.write(f"{fen} | {cp} | {result}\n")
                n_pos += 1
            n_games += 1
            el = time.time() - t0
            print(f"game {n_games}/{args.games}: result={result} "
                  f"positions={n_pos} elapsed={el:.0f}s", flush=True)
    finally:
        out.close()
    for e in players + [teach]:
        try:
            e.quit()
        except Exception:
            pass
    meta = {
        "games": n_games,
        "positions": n_pos,
        "teacher": args.teacher_name,
        "teacher_id": tid,
        "depth": args.depth,
        "movetime_ms": args.movetime,
        "generator": "tools/gen_data.py",
    }
    try:
        with open(args.teacher, "rb") as f:
            meta["teacher_sha256"] = hashlib.sha256(f.read()).hexdigest()
    except OSError:
        pass
    with open(args.out + ".meta.json", "w") as f:
        json.dump(meta, f, indent=2)
    print(f"wrote {args.out} ({n_pos} positions from {n_games} games)")
    print(json.dumps(meta, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
