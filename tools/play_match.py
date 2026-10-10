#!/usr/bin/env python3
"""Dev match runner (python-chess). Plays N games between two UCI engines.

- Openings: embedded 24-line book (UCI move lists from startpos), each
  played twice with colors reversed (paired).
- Time control: --movetime MS per move, or --tc BASE(+INC) seconds.
- End: mate / stalemate / 50-move / 3fold / 250-move cap (draw).
- Crashes/time-forfeits/illegal moves count as LOSSES, never dropped.
- Writes PGN + summary line parsable for Elo: FINAL W=x D=y L=z.
Requires: python-chess (pip install python-chess).
"""
import argparse
import datetime
import sys
import time

OPENINGS = [
    "e2e4 e7e5 g1f3 b8c6 f1b5",                                   # Ruy Lopez
    "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5",                             # Italian
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6",          # Najdorf
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 g7g6",          # Dragon
    "e2e4 e7e6 d2d4 d7d5 b1c3 f8b4",                             # French Winawer
    "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4",                        # Caro-Kann
    "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7",                   # QGD
    "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 g1f3 e7e5",          # KID
    "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4",                             # Nimzo-Indian
    "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 d5c4",                   # Slav
    "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6",                             # English
    "g1f3 d7d5 c2c4 d5c4",                                       # Reti
    "d2d4 d7d5 c1f4 g8f6 e2e3 e7e6 g1f3",                        # London
    "e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4",                        # Scotch
    "e2e4 e7e5 g1f3 g8f6 f3e5 d7d6",                             # Petroff
    "e2e4 d7d5 e4d5 d8d5 b1c3 d5a5",                             # Scandinavian
    "e2e4 d7d6 d2d4 g8f6 b1c3 g7g6",                             # Pirc
    "e2e4 g7g6 d2d4 f8g7",                                       # Modern
    "d2d4 f7f5 g2g3 g8f6 f1g2 e7e6",                             # Dutch
    "d2d4 g8f6 c2c4 c7c5 d4d5 e7e6",                             # Benoni
    "d2d4 g8f6 c2c4 g7g6 b1c3 d7d5 c4d5 f6d5",                   # Gruenfeld
    "e2e4 e7e5 b1c3 g8f6 f2f4",                                  # Vienna Gambit
    "e2e4 e7e5 f2f4 e5f4 g1f3",                                  # KGA
    "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 b2b4",                        # Evans
]


def parse_tc(s):
    if "+" in s:
        base, inc = s.split("+")
        return float(base), float(inc)
    return float(s), 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--white", required=True)
    ap.add_argument("--black", required=True)
    ap.add_argument("--games", type=int, default=48)
    ap.add_argument("--movetime", type=int, default=0, help="ms per move")
    ap.add_argument("--nodes", type=int, default=0,
                    help="fixed nodes per move (deterministic, clock-independent)")
    ap.add_argument("--tc", default="", help="base+inc seconds, e.g. 10+0.1")
    ap.add_argument("--threads", type=int, default=1)
    ap.add_argument("--hash", type=int, default=32)
    ap.add_argument("--pgn", default="")
    ap.add_argument("--name-white", default="white")
    ap.add_argument("--name-black", default="black")
    ap.add_argument("--opt", action="append", default=[],
                    help="extra UCI option Name=value for BOTH engines")
    ap.add_argument("--opt-a", action="append", default=[],
                    help="extra UCI option for engine A (--white binary only)")
    ap.add_argument("--opt-b", action="append", default=[],
                    help="extra UCI option for engine B (--black binary only)")
    args = ap.parse_args()

    import chess
    import chess.engine
    import chess.pgn

    base, inc = parse_tc(args.tc) if args.tc else (0.0, 0.0)
    use_mtime = args.movetime > 0
    use_nodes = args.nodes > 0

    # validate openings once
    books = []
    for line in OPENINGS:
        b = chess.Board()
        for u in line.split():
            try:
                b.push_uci(u)
            except ValueError:
                print(f"book ERROR illegal {u} in: {line}")
                return 2
        books.append(line)

    w_eng = chess.engine.SimpleEngine.popen_uci(args.white)
    b_eng = chess.engine.SimpleEngine.popen_uci(args.black)
    def parse_opts(items):
        d = {}
        for o in items:
            if "=" in o:
                k, v = o.split("=", 1)
                d[k] = True if v == "true" else (False if v == "false" else v)
        return d

    common = {"Threads": args.threads, "Hash": args.hash}
    common.update(parse_opts(args.opt))
    opts_a = dict(common)
    opts_a.update(parse_opts(args.opt_a))
    opts_b = dict(common)
    opts_b.update(parse_opts(args.opt_b))
    w_eng.configure(opts_a)
    b_eng.configure(opts_b)

    pgn_games = []
    W = D = L = 0
    t_start = time.time()
    # Pairing: games (2k, 2k+1) share book k with COLORS REVERSED.
    # Score W/D/L is always from the --white BINARY's perspective.
    for g in range(args.games):
        book = books[(g // 2) % len(books)]
        a_is_white = (g % 2 == 0)  # engine A (--white) has White on even games
        board = chess.Board()
        for u in book.split():
            board.push_uci(u)
        we = w_eng if a_is_white else b_eng
        be = b_eng if a_is_white else w_eng
        wname = args.name_white if a_is_white else args.name_black
        bname = args.name_black if a_is_white else args.name_white
        wtime = btime = base * 1000.0
        winc = binc = inc * 1000.0
        result = None
        termination = ""
        moves_made = 0
        while not board.is_game_over(claim_draw=True) and board.ply() < 500:
            turn_white = board.turn == chess.WHITE
            eng = we if turn_white else be
            try:
                if use_nodes:
                    lim = chess.engine.Limit(nodes=args.nodes)
                elif use_mtime:
                    lim = chess.engine.Limit(time=args.movetime / 1000.0)
                else:
                    lim = chess.engine.Limit(
                        white_clock=wtime / 1000.0, black_clock=btime / 1000.0,
                        white_inc=winc / 1000.0, black_inc=binc / 1000.0)
                t0 = time.time()
                res = eng.play(board, lim)
                spent = (time.time() - t0) * 1000.0
            except (chess.engine.EngineError, chess.engine.EngineTerminatedError) as e:
                # crash / illegal move / timeout-termination: game loss
                result = "0-1" if turn_white else "1-0"
                termination = f"engine failure ({type(e).__name__})"
                break
            if res.move is None or res.move not in board.legal_moves:
                result = "0-1" if turn_white else "1-0"
                termination = "illegal/no move"
                break
            if turn_white:
                wtime = max(0.0, wtime - spent + winc)
            else:
                btime = max(0.0, btime - spent + binc)
            if not use_mtime and not use_nodes and ((turn_white and wtime <= 0)
                                  or (not turn_white and btime <= 0)):
                result = "0-1" if turn_white else "1-0"
                termination = "time forfeit"
                break
            board.push(res.move)
            moves_made += 1
        if result is None:
            if board.is_checkmate():
                result = "0-1" if board.turn == chess.WHITE else "1-0"
                termination = "checkmate"
            elif (board.is_stalemate() or board.is_insufficient_material()
                    or board.is_seventyfive_moves()
                    or board.is_fivefold_repetition()
                    or board.ply() >= 500):
                result = "1/2-1/2"
                termination = "draw rule / move cap"
            elif board.can_claim_draw():
                result = "1/2-1/2"
                termination = "draw claim"
            else:
                result = "1/2-1/2"
                termination = "undecided->draw"
        # score from the --white binary's perspective (engine A)
        a_white = a_is_white
        if result == "1/2-1/2":
            D += 1
        elif (result == "1-0") == a_white:
            W += 1
        else:
            L += 1
        game = chess.pgn.Game.from_board(board)
        game.headers["White"] = wname
        game.headers["Black"] = bname
        game.headers["Result"] = result
        game.headers["Termination"] = termination
        game.headers["Date"] = datetime.date.today().strftime("%Y.%m.%d")
        game.headers["WhiteEngine"] = args.white
        game.headers["BlackEngine"] = args.black
        pgn_games.append(game)
        el = time.time() - t_start
        print(f"game {g+1}/{args.games}: {wname}-{bname} {result} "
              f"({termination}) [{moves_made} moves] score(A) W{W} D{D} L{L} "
              f"elapsed {el:.0f}s", flush=True)
    for e in (w_eng, b_eng):
        try:
            e.quit()
        except Exception:
            pass
    if args.pgn:
        with open(args.pgn, "w") as f:
            for game in pgn_games:
                print(game, file=f, end="\n\n")
        print(f"wrote {args.pgn}")
    print(f"FINAL W={W} D={D} L={L}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
